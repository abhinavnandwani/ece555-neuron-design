// ECE555 neuron: whole-design model with FinFET switch-level timing, calibrated to ASAP7.
//
//   out = ReLU(x1*w1 + x2*w2 + wb)        2-bit codes: 00=0, 01=+1, 11=-1 (10 unused)
//
// Devices
//   Every gate is static CMOS: a pull-down (PDN) series/parallel tree and its dual pull-up.
//   Device size is an integer number of fins. Two sizing schemes:
//     min    every transistor 1 fin
//     stack  series stacks compensated (worst path matches a 1-fin NMOS / beta-fin PMOS
//            inverter), widths rounded up to whole fins; beta = stack_beta (1.5 by default)
//   The optimizer then adds whole fins (an integer multiplier per gate) on the critical path.
//
// Timing (Elmore, event driven, inertial), in ps after calibration:
//   on-resistance per device  NMOS 1/fins, PMOS mu/fins          (units of R_fin)
//   gate cap                  1 per fin                           (units of C_fin)
//   output diffusion          gamma per fin of devices on the output node
//   stack-node diffusion      gint per fin of devices on each internal series node
//   delay = ln2 * tau * (R_path * C_out + sum over internal nodes C_node * R_node_to_rail)
//   Inputs are driven by 1N/2P inverters like the Spectre testbench; a transition's delay is
//   last output change minus the earliest input-pin change, matching the measurement.
//
// Calibration (calibration/asap7_tt_25c_0p9v.txt): 4,896 Spectre transitions over six circuits
// (spec/carry-save XOR/carry-save mirror, each at min and stack sizing), ASAP7 RVT, TT, 25 C,
// 0.9 V, L = 20 nm, 1 fF per output. `--calibrate FILE` refits tau, mu, gamma, gint, cload.
// Pre-layout only: no wires, corners or real layout area.
//
// Area
//   fins       total fin count (what the simulations measured)
//   cell area  estimate for one uniform row height, chosen per design. A row with r fins per
//              device is (2r + 4) * 27 nm tall: r = 3 gives ASAP7's 270 nm 7.5-track row, r = 1
//              gives 162 nm. A device wider than r folds into ceil(fins / r) columns. Per gate:
//              columns = max(NMOS columns, PMOS columns) + 1 diffusion break, at 54 nm CPP.
//              The smallest of r = 1..3 is reported. Excludes routing and whitespace, so it is a
//              ranking aid, not the graded boundary area.
//
// Usage: ./neuron                       report (calibrated defaults)
//        ./neuron --json out.json        also export netlists for the diagram page
//        ./neuron --calibrate FILE       refit timing parameters to Spectre measurements
//        --tau --mu --gamma --gint --cload --stack_beta   override parameters

#include <bits/stdc++.h>
#include <future>
using namespace std;

struct Params {
    // Fit B in calibration/fit.txt: fitted to 4,896 ASAP7 Spectre transitions with mu fixed at
    // the measured single-bias drain-current ratio, 49.35 uA / 41.19 uA = 1.20.
    // Per-transition RMS error 12.7%; worst-case delay of each circuit within 10%.
    double tau = 0.590, mu = 1.2, gamma = 1.135, gint = 1.027, cload = 19.90;
    double ks = 0.520;                     // series stack: m devices act like R*(1+(m-1)*ks)/m each (1 = linear)
    double stack_beta = 1.5;               // P/N ratio used by the stack sizing scheme
    int drvN = 1, drvP = 2;                // input driver inverter, fins (testbench uses 1N/2P)
    int fmax = 3; double cpp_nm = 54, fin_pitch_nm = 27;  // area estimate: row = (2*fins + 4) fin pitches
};
enum Sizing { SZ_MIN, SZ_STACK };
static const char* SZN[] = {"min", "stack"};

// ---------------------------------------------------------------- SP networks
struct Node {
    enum K { LEAF, SER, PAR } k;
    int pin = -1;
    double w = 0;           // fins
    vector<Node> ch;
    vector<double> jw;      // SER: fins on each junction between consecutive children
};
static Node L(int p) { Node n; n.k = Node::LEAF; n.pin = p; return n; }
static Node S(vector<Node> c) { Node n; n.k = Node::SER; n.ch = move(c); return n; }
static Node P(vector<Node> c) { Node n; n.k = Node::PAR; n.ch = move(c); return n; }
static Node dual(const Node& n) {
    Node d = n;
    if (n.k == Node::SER) d.k = Node::PAR; else if (n.k == Node::PAR) d.k = Node::SER;
    for (auto& c : d.ch) c = dual(c);
    return d;
}
static int slen(const Node& n) {
    if (n.k == Node::LEAF) return 1;
    int r = 0;
    for (auto& c : n.ch) r = (n.k == Node::SER) ? r + slen(c) : max(r, slen(c));
    return r;
}
static void size_stack(Node& n, double R, double scale) {  // continuous stack compensation
    if (n.k == Node::LEAF) { n.w = scale / R; return; }
    int tot = slen(n);
    for (auto& c : n.ch) size_stack(c, n.k == Node::SER ? R * slen(c) / tot : R, scale);
}
static void to_fins(Node& n, bool minimum, int mult) {
    if (n.k == Node::LEAF) { n.w = mult * (minimum ? 1.0 : ceil(n.w - 1e-9)); return; }
    for (auto& c : n.ch) to_fins(c, minimum, mult);
}
static double top_width(const Node& n) {
    if (n.k == Node::LEAF) return n.w;
    if (n.k == Node::SER) return top_width(n.ch.front());
    double r = 0; for (auto& c : n.ch) r += top_width(c); return r;
}
static double bot_width(const Node& n) {
    if (n.k == Node::LEAF) return n.w;
    if (n.k == Node::SER) return bot_width(n.ch.back());
    double r = 0; for (auto& c : n.ch) r += bot_width(c); return r;
}
static void prep_junctions(Node& n) {
    if (n.k == Node::LEAF) return;
    for (auto& c : n.ch) prep_junctions(c);
    n.jw.clear();
    if (n.k == Node::SER) for (size_t i = 0; i + 1 < n.ch.size(); i++) n.jw.push_back(bot_width(n.ch[i]) + top_width(n.ch[i + 1]));
}
static int count_dev(const Node& n) { if (n.k == Node::LEAF) return 1; int r = 0; for (auto& c : n.ch) r += count_dev(c); return r; }
static double sum_w(const Node& n) { if (n.k == Node::LEAF) return n.w; double r = 0; for (auto& c : n.ch) r += sum_w(c); return r; }
static int columns(const Node& n, int fmax) { if (n.k == Node::LEAF) return (int)ceil(n.w / fmax - 1e-9); int r = 0; for (auto& c : n.ch) r += columns(c, fmax); return r; }
static void pin_widths(const Node& n, vector<double>& acc) { if (n.k == Node::LEAF) { acc[n.pin] += n.w; return; } for (auto& c : n.ch) pin_widths(c, acc); }
static const double INF = 1e18;
static bool conducts(const Node& n, const int* v, bool pmos) {
    if (n.k == Node::LEAF) return pmos ? !v[n.pin] : v[n.pin];
    if (n.k == Node::SER) { for (auto& c : n.ch) if (!conducts(c, v, pmos)) return false; return true; }
    for (auto& c : n.ch) if (conducts(c, v, pmos)) return true;
    return false;
}
struct EV { double R, M, C; };  // resistance, sum C_int * R_to_rail, internal cap
static EV elmore(const Node& n, const int* v, bool pmos, double mu, double gint, double ks) {
    if (n.k == Node::LEAF) {
        bool on = pmos ? !v[n.pin] : v[n.pin];
        return {on ? (pmos ? mu : 1.0) / n.w : INF, 0, 0};
    }
    if (n.k == Node::SER) {
        int m = n.ch.size(); vector<EV> e(m);
        for (int i = 0; i < m; i++) { e[i] = elmore(n.ch[i], v, pmos, mu, gint, ks); if (e[i].R >= INF) return {INF, 0, 0}; }
        double f = (1 + (m - 1) * ks) / m;
        for (auto& x : e) x.R *= f;
        double Rb = 0, M = 0, C = 0;
        for (int i = m - 1; i >= 0; i--) {   // child 0 is nearest the output
            M += e[i].M + e[i].C * Rb; C += e[i].C;
            if (i < m - 1) { double cj = gint * n.jw[i]; M += cj * Rb; C += cj; }
            Rb += e[i].R;
        }
        return {Rb, M, C};
    }
    double g = 0, M = 0, C = 0;
    for (auto& c : n.ch) { EV x = elmore(c, v, pmos, mu, gint, ks); if (x.R < INF) { g += 1 / x.R; M += x.M; C += x.C; } }
    return g > 0 ? EV{1 / g, M, C} : EV{INF, 0, 0};
}

// ------------------------------------------------------------ primitives
struct Prim { string name; int npins; Node pdn; vector<string> pins; };
static Prim INV_{"INV", 1, L(0), {"a"}};
static Prim NAND2_{"NAND2", 2, S({L(0), L(1)}), {"a", "b"}};
static Prim NOR2_{"NOR2", 2, P({L(0), L(1)}), {"a", "b"}};
static Prim NOR3_{"NOR3", 3, P({L(0), L(1), L(2)}), {"a", "b", "c"}};
static Prim AOI21_{"AOI21", 3, P({S({L(0), L(1)}), L(2)}), {"a", "b", "c"}};             // !(ab+c)
static Prim OAI21_{"OAI21", 3, S({P({L(0), L(1)}), L(2)}), {"a", "b", "c"}};             // !((a+b)c)
static Prim AOI211_{"AOI211", 4, P({S({L(0), L(1)}), L(2), L(3)}), {"a", "b", "c", "d"}}; // !(ab+c+d)
static Prim AOI22_{"AOI22", 4, P({S({L(0), L(1)}), S({L(2), L(3)})}), {"a", "b", "c", "d"}};
static Prim CXOR_{"XOR8", 4, P({S({L(0), L(1)}), S({L(2), L(3)})}), {"a", "b", "an", "bn"}};   // a^b
static Prim CXNOR_{"XNOR8", 4, P({S({L(0), L(3)}), S({L(2), L(1)})}), {"a", "b", "an", "bn"}}; // !(a^b)
static Prim MCARRY_{"MIR_CO", 3, P({S({L(0), L(1)}), S({L(2), P({L(0), L(1)})})}), {"a", "b", "c"}};
static Prim MSUM_{"MIR_S", 4, P({S({L(0), L(1), L(2)}), S({L(3), P({L(0), L(1), L(2)})})}), {"a", "b", "c", "cob"}};

// --------------------------------------------------------------- circuit
struct Gate {
    const Prim* prim; vector<int> in; int out; string label; int mult = 1;
    Node pdn, pun; int ndev = 0; vector<double> pinC; double diff = 0, fins = 0; int cols[4] = {0, 0, 0, 0};
};

struct Circuit {
    Params p; Sizing sz;
    vector<string> netName; vector<int> driver; vector<vector<int>> fanout;
    vector<Gate> gates; vector<int> inputs, outputs;
    int GND, VDD; string prefix;
    map<int, int> invCache; map<tuple<int, int, int>, int> xorCache; map<int, string> alias;
    vector<vector<string>> blocks;
    vector<double> netC;

    Circuit(Params pp, Sizing s) : p(pp), sz(s) { GND = net("GND"); VDD = net("VDD"); }
    int net(const string& n) { netName.push_back(n); driver.push_back(-1); fanout.emplace_back(); return (int)netName.size() - 1; }
    int input(const string& n) { int x = net(n); inputs.push_back(x); return x; }
    void applySize(Gate& g) {
        g.pdn = g.prim->pdn; g.pun = dual(g.prim->pdn);
        size_stack(g.pdn, 1.0, 1.0); size_stack(g.pun, 1.0, p.stack_beta);
        to_fins(g.pdn, sz == SZ_MIN, g.mult); to_fins(g.pun, sz == SZ_MIN, g.mult);
        prep_junctions(g.pdn); prep_junctions(g.pun);
        g.ndev = count_dev(g.pdn) + count_dev(g.pun);
        g.pinC.assign(g.prim->npins, 0); pin_widths(g.pdn, g.pinC); pin_widths(g.pun, g.pinC);
        g.diff = top_width(g.pdn) + top_width(g.pun);
        g.fins = sum_w(g.pdn) + sum_w(g.pun);
        for (int r = 1; r <= 3; r++) g.cols[r] = max(columns(g.pdn, r), columns(g.pun, r)) + 1;
    }
    int gate(const Prim& pr, vector<int> in) {
        Gate g; g.prim = &pr; g.in = in;
        g.label = prefix + pr.name + "#" + to_string(gates.size());
        g.out = net(g.label);
        applySize(g);
        int id = gates.size();
        driver[g.out] = id;
        for (int x : in) fanout[x].push_back(id);
        gates.push_back(g);
        return gates.back().out;
    }
    int inv(int a) {
        auto it = invCache.find(a); if (it != invCache.end()) return it->second;
        int y = gate(INV_, {a}); invCache[a] = y; invCache[y] = a; return y;
    }
    void finalize() {
        netC.assign(netName.size(), 0);
        for (auto& g : gates) {
            for (int i = 0; i < (int)g.in.size(); i++) netC[g.in[i]] += g.pinC[i];
            netC[g.out] += p.gamma * g.diff;
        }
        for (int x : inputs) netC[x] += p.gamma * (p.drvN + p.drvP);
        for (int o : outputs) netC[o] += p.cload;
    }
    void setMult(int gi, int m) { gates[gi].mult = m; applySize(gates[gi]); }
    int transistors() const { int n = 0; for (auto& g : gates) n += g.ndev; return n; }
    double fins() const { double a = 0; for (auto& g : gates) a += g.fins; return a; }
    double rowArea(int r) const { int c = 0; for (auto& g : gates) c += g.cols[r]; return c * p.cpp_nm * (2 * r + 4) * p.fin_pitch_nm * 1e-6; }
    int row() const { int b = 1; for (int r = 2; r <= p.fmax; r++) if (rowArea(r) < rowArea(b)) b = r; return b; }
    double area_um2() const { return rowArea(row()); }
};

// ------------------------------------------------------------ design space
enum Arch { A_SPEC, A_CSA };
enum XorStyle { X_NAND4, X_CMOS12, X_AOI10 };
enum FaStyle { FA_XOR, FA_MIRROR };
enum ReluStyle { R_MUX, R_AND, R_NONE };
enum MulStyle { M_FIXED, M_SCREENSHOT };
static const char* AN[] = {"Spec datapath", "Carry-save count"};
static const char* XN[] = {"NAND4 (16T)", "CMOS12", "NOR+AOI (10T)"};
static const char* FN[] = {"XOR-based FA", "Mirror FA"};
static const char* RN[] = {"3-bit MUX", "AND gating", "built into output gates"};
static const char* MN[] = {"fixed", "screenshot"};
struct Config { Arch a; XorStyle x; FaStyle fa; ReluStyle r; MulStyle m; };

struct Sig { int n; bool inv; };  // logical value = inv ? !net : net

struct Builder {
    Circuit& c; Config cf;
    int NAND(int a, int b) { return c.gate(NAND2_, {a, b}); }
    int NOR(int a, int b) { return c.gate(NOR2_, {a, b}); }
    int AND(int a, int b) { return c.inv(NAND(a, b)); }
    int T(Sig s) { return s.inv ? c.inv(s.n) : s.n; }
    int B(Sig s) { return s.inv ? s.n : c.inv(s.n); }
    void A(int n, const string& nm) { if (n != c.GND && !c.alias.count(n)) c.alias[n] = nm; }
    void A(Sig s, const string& nm) { A(s.n, s.inv ? "~" + nm : nm); }
    int XORg(int a, int b, bool xnor) {
        auto key = make_tuple(min(a, b), max(a, b), (int)xnor);
        auto it = c.xorCache.find(key); if (it != c.xorCache.end()) return it->second;
        int y;
        if (cf.x == X_NAND4) {
            if (!xnor) { int n1 = NAND(a, b); y = NAND(NAND(a, n1), NAND(b, n1)); }
            else { int n1 = NOR(a, b); y = NOR(NOR(a, n1), NOR(b, n1)); }
        } else if (cf.x == X_CMOS12) {
            int an = c.inv(a), bn = c.inv(b);
            y = c.gate(xnor ? CXNOR_ : CXOR_, {a, b, an, bn});
        } else {
            y = xnor ? c.gate(OAI21_, {a, b, NAND(a, b)}) : c.gate(AOI21_, {a, b, NOR(a, b)});
        }
        return c.xorCache[key] = y;
    }
    int XOR(int a, int b) { return XORg(a, b, false); }
    int XNOR(int a, int b) { return XORg(a, b, true); }
    pair<int, int> HA(int a, int b) { return {XOR(a, b), AND(a, b)}; }
    pair<Sig, Sig> FA(int a, int b, int ci) {  // {sum, carry}
        if (cf.fa == FA_XOR) {
            int pp = XOR(a, b), s = XOR(pp, ci), co = NAND(NAND(a, b), NAND(pp, ci));
            return {{s, false}, {co, false}};
        }
        int cob = c.gate(MCARRY_, {a, b, ci});
        int sb = c.gate(MSUM_, {a, b, ci, cob});
        return {{sb, true}, {cob, true}};  // mirror adder gives inverted outputs for free
    }
    pair<int, int> MUL(int x1, int x0, int w1, int w0) {  // {y1, y0}; y0 = nonzero, y1 = negative
        if (cf.m == M_FIXED) {
            int n = NAND(x0, w0);
            return {NOR(n, XNOR(x1, w1)), c.inv(n)};
        }
        int zb = c.inv(NOR(w1, w0)), nn = AND(w1, w0);  // equations from the screenshot
        return {AND(XOR(x1, nn), zb), AND(x0, zb)};
    }
    vector<int> build(int x1[2], int x2[2], int w1[2], int w2[2], int wb[2]) {
        c.prefix = "MUL1/"; auto p1 = MUL(x1[1], x1[0], w1[1], w1[0]);
        c.prefix = "MUL2/"; auto p2 = MUL(x2[1], x2[0], w2[1], w2[0]);
        A(p1.first, "p1[1]"); A(p1.second, "p1[0]"); A(p2.first, "p2[1]"); A(p2.second, "p2[0]");
        vector<int> o(3, c.GND);
        if (cf.a == A_SPEC) {
            c.blocks = {{"MUL1", "MUL2"}, {"ADD2"}, {"ADD3"}, {"RELU"}};
            c.prefix = "ADD2/";
            auto h0 = HA(p1.second, p2.second);
            auto f1 = FA(p1.first, p2.first, h0.second);
            int c2 = T(f1.second);
            int s[3] = {h0.first, T(f1.first), XOR(XOR(p1.first, p2.first), c2)};
            A(h0.second, "add2.c1"); A(c2, "add2.c2"); A(s[0], "s[0]"); A(s[1], "s[1]"); A(s[2], "s[2]");
            c.prefix = "ADD3/";
            auto g0 = HA(s[0], wb[0]);
            auto g1 = FA(s[1], wb[1], g0.second);
            int d2 = T(g1.second);
            int t[3] = {g0.first, T(g1.first), XOR(XOR(s[2], wb[1]), d2)};
            A(g0.second, "add3.c1"); A(d2, "add3.c2"); A(t[0], "t[0]"); A(t[1], "t[1]"); A(t[2], "t[2] sign");
            c.prefix = "RELU/";
            if (cf.r == R_MUX) {
                int sn = c.inv(t[2]);
                for (int i = 0; i < 3; i++) o[i] = c.inv(c.gate(AOI22_, {t[i], sn, c.GND, t[2]}));
            } else {
                for (int i = 0; i < 2; i++) o[i] = NOR(c.inv(t[i]), t[2]);
            }
        } else {
            // v = y0 - 2*y1 for each product and the bias. Sum = (#nonzero) - 2*(#negative).
            // #nonzero = s0 + 2*k1, #negative = n0 + 2*n1  ->  Sum = s0 + 2*(k1 - n0 - 2*n1)
            // out[1] = k1 & !n0 & !n1          out[0] = s0 & !n1 & (k1 | !n0)
            c.blocks = {{"MUL1", "MUL2"}, {"CNT"}, {"OUT"}};
            c.prefix = "CNT/";
            auto nz = FA(p1.second, p2.second, wb[0]);
            auto ng = FA(p1.first, p2.first, wb[1]);
            A(nz.first, "nz.sum"); A(nz.second, "nz.carry"); A(ng.first, "neg.sum"); A(ng.second, "neg.carry");
            c.prefix = "OUT/";
            int k1b = B(nz.second), s0b = B(nz.first), n0 = T(ng.first), n1 = T(ng.second);
            o[1] = c.gate(NOR3_, {k1b, n0, n1});
            o[0] = c.gate(AOI211_, {k1b, n0, s0b, n1});
        }
        for (int i = 0; i < 3; i++) A(o[i], "out[" + to_string(i) + "]");
        c.prefix = "";
        return o;
    }
};

// ----------------------------------------------------------- simulation
static const int CODES[3] = {0b00, 0b01, 0b11};
static int dec2(int c) { return c == 0b00 ? 0 : c == 0b01 ? 1 : -1; }
static int enc2(int v) { return v == 0 ? 0b00 : v == 1 ? 0b01 : 0b11; }
struct Vec { int x1, x2, w1, w2, wb; };
static int reference(const Vec& v) { return max(0, dec2(v.x1) * dec2(v.w1) + dec2(v.x2) * dec2(v.w2) + dec2(v.wb)); }
static string vstr(const Vec& v) {
    char b[96]; snprintf(b, sizeof b, "x1=%+d x2=%+d w1=%+d w2=%+d wb=%+d", dec2(v.x1), dec2(v.x2), dec2(v.w1), dec2(v.w2), dec2(v.wb)); return b;
}
static vector<Vec> allVecs() {
    vector<Vec> v;
    for (int a : CODES) for (int b : CODES) for (int c : CODES) for (int d : CODES) for (int e : CODES) v.push_back({a, b, c, d, e});
    return v;
}
static const vector<Vec> VECS = allVecs();
// Transitions that change the output with the weights held fixed: what a testbench can apply
// and what the Spectre sweep measured (816 of them).
static vector<pair<int, int>> makePairs() {
    vector<pair<int, int>> p;
    for (int i = 0; i < (int)VECS.size(); i++) for (int j = 0; j < (int)VECS.size(); j++) {
        const Vec &a = VECS[i], &b = VECS[j];
        if (i != j && a.w1 == b.w1 && a.w2 == b.w2 && a.wb == b.wb && reference(a) != reference(b)) p.push_back({i, j});
    }
    return p;
}
static const vector<pair<int, int>> PAIRS = makePairs();

struct Design {
    Circuit c; int in[10]; vector<int> o; Config cf;
    Design(Config cf_, Params p, Sizing s) : c(p, s), cf(cf_) {
        const char* nm[5] = {"x1", "x2", "w1", "w2", "wb"};
        for (int i = 0; i < 5; i++) { in[2 * i] = c.input(string(nm[i]) + "[0]"); in[2 * i + 1] = c.input(string(nm[i]) + "[1]"); }
        int X1[2] = {in[0], in[1]}, X2[2] = {in[2], in[3]}, W1[2] = {in[4], in[5]}, W2[2] = {in[6], in[7]}, WB[2] = {in[8], in[9]};
        Builder b{c, cf};
        o = b.build(X1, X2, W1, W2, WB);
        for (int x : o) if (x != c.GND) c.outputs.push_back(x);
        c.finalize();
    }
};

struct Sim {
    Circuit& c; const int* in; const vector<int>& o;
    vector<int> val, cur, pv, pseq; vector<double> tChange; vector<int> tCause, cause;
    Sim(Design& d) : c(d.c), in(d.in), o(d.o) {}
    int gin[8];
    void loadIn(const Gate& g) { for (size_t i = 0; i < g.in.size(); i++) gin[i] = val[g.in[i]]; }
    int evalGate(const Gate& g) { loadIn(g); return conducts(g.pdn, gin, false) ? 0 : 1; }
    double delay(const Gate& g, int nv) {
        loadIn(g);
        EV e = nv ? elmore(g.pun, gin, true, c.p.mu, c.p.gint, c.p.ks) : elmore(g.pdn, gin, false, c.p.mu, c.p.gint, c.p.ks);
        return M_LN2 * c.p.tau * (e.R * c.netC[g.out] + e.M);
    }
    void setInputs(const Vec& v) {
        int codes[5] = {v.x1, v.x2, v.w1, v.w2, v.wb};
        for (int i = 0; i < 5; i++) { val[in[2 * i + 1]] = (codes[i] >> 1) & 1; val[in[2 * i]] = codes[i] & 1; }
    }
    void settle(const Vec& v) {
        val.assign(c.netName.size(), 0); val[c.VDD] = 1;
        setInputs(v);
        for (auto& g : c.gates) val[g.out] = evalGate(g);
        int G = c.gates.size();
        cur.resize(G); for (int i = 0; i < G; i++) cur[i] = val[c.gates[i].out];
        pv.assign(G, -1); pseq.assign(G, 0); cause.assign(G, -1);
        tChange.assign(c.netName.size(), -1); tCause.assign(c.netName.size(), -1);
    }
    int readOut() { return val[o[0]] | (val[o[1]] << 1) | (val[o[2]] << 2); }
    // Delay from the earliest input-pin change to the last output change (Spectre convention).
    double transition(const Vec& a, const Vec& b, int& lastNet) {
        settle(a);
        using E = tuple<double, int, int>;
        priority_queue<E, vector<E>, greater<E>> q;
        auto evaluate = [&](int gi, double t, int cn) {
            const Gate& g = c.gates[gi];
            int nv = evalGate(g);
            if (pv[gi] != -1) { if (nv == pv[gi]) return; pv[gi] = -1; pseq[gi]++; if (nv == cur[gi]) return; }
            if (nv == cur[gi]) return;
            pv[gi] = nv; cause[gi] = cn;
            q.push({t + delay(g, nv), pseq[gi], gi});
        };
        int codes[5] = {b.x1, b.x2, b.w1, b.w2, b.wb};
        int target[10]; double first = INF;
        for (int k = 0; k < 10; k++) {
            target[k] = (codes[k / 2] >> (k % 2)) & 1;
            int n = in[k];
            if (target[k] != val[n]) {
                double R = target[k] ? c.p.mu / c.p.drvP : 1.0 / c.p.drvN;
                double t = M_LN2 * c.p.tau * R * c.netC[n];
                first = min(first, t);
                q.push({t, 0, -(k + 1)});
            }
        }
        double last = -1; lastNet = -1;
        while (!q.empty()) {
            auto [t, s, id] = q.top(); q.pop();
            int changed;
            if (id < 0) {
                int k = -id - 1; changed = in[k];
                val[changed] = target[k]; tChange[changed] = t; tCause[changed] = -1;
            } else {
                if (s != pseq[id] || pv[id] == -1) continue;
                const Gate& g = c.gates[id];
                cur[id] = pv[id]; pv[id] = -1; pseq[id]++;
                changed = g.out; val[changed] = cur[id]; tChange[changed] = t; tCause[changed] = cause[id];
                for (int x : c.outputs) if (x == changed) { last = t; lastNet = changed; }
            }
            for (int fo : c.fanout[changed]) evaluate(fo, t, changed);
        }
        if (last < 0) return -1;
        return last - first;
    }
    vector<pair<int, double>> tracePath(int net) {
        vector<pair<int, double>> p;
        while (net >= 0) { p.push_back({net, tChange[net]}); net = tCause[net]; }
        reverse(p.begin(), p.end()); return p;
    }
};

struct Timing {
    double worst = 0, rise = 0, fall = 0;
    int wi = -1, wj = -1;
    vector<pair<int, double>> path;
    vector<int> topPairs;
};
static Timing timeIt(Design& d, const vector<int>* subset, int keepTop) {
    Sim s(d); Timing T;
    vector<pair<double, int>> all;
    int n = subset ? subset->size() : PAIRS.size();
    for (int q = 0; q < n; q++) {
        int pi = subset ? (*subset)[q] : q;
        auto [i, j] = PAIRS[pi];
        int lastNet;
        double t = s.transition(VECS[i], VECS[j], lastNet);
        if (s.readOut() != reference(VECS[j])) { fprintf(stderr, "settle mismatch\n"); exit(1); }
        if (t < 0) continue;
        if (keepTop) all.push_back({t, pi});
        if (s.val[lastNet]) T.rise = max(T.rise, t); else T.fall = max(T.fall, t);
        if (t > T.worst) { T.worst = t; T.wi = i; T.wj = j; T.path = s.tracePath(lastNet); }
    }
    if (keepTop) {
        int k = min(keepTop, (int)all.size());
        partial_sort(all.begin(), all.begin() + k, all.end(), greater<>());
        for (int q = 0; q < k; q++) T.topPairs.push_back(all[q].second);
    }
    return T;
}
static int funcErrors(Design& d, vector<Vec>* ex = nullptr) {
    Sim s(d); int e = 0;
    for (auto& v : VECS) { s.settle(v); if (s.readOut() != reference(v)) { e++; if (ex && ex->size() < 4) ex->push_back(v); } }
    return e;
}

// ------------------------------------------------------------ calibration
struct Meas { int design; Vec a, b; double ps; };
static const char* CAL_DESIGNS[] = {"spec_xor", "csa_xor", "csa_mirror"};
static Config calConfig(int k) {
    if (k == 0) return {A_SPEC, X_AOI10, FA_XOR, R_AND, M_FIXED};
    if (k == 1) return {A_CSA, X_AOI10, FA_XOR, R_NONE, M_FIXED};
    return {A_CSA, X_AOI10, FA_MIRROR, R_NONE, M_FIXED};
}
static vector<Meas> loadMeas(const string& path) {
    ifstream f(path); string line; vector<Meas> m;
    if (!f) { fprintf(stderr, "cannot open %s\n", path.c_str()); exit(1); }
    while (getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        istringstream is(line); string arch, sz; int a[5], b[5]; double ps;
        is >> arch >> sz; for (int& x : a) is >> x; for (int& x : b) is >> x; is >> ps;
        int k = -1; for (int i = 0; i < 3; i++) if (arch == CAL_DESIGNS[i]) k = i;
        if (k < 0 || !is) continue;
        Meas r; r.design = k * 2 + (sz == "stack");
        r.a = {enc2(a[0]), enc2(a[1]), enc2(a[2]), enc2(a[3]), enc2(a[4])};
        r.b = {enc2(b[0]), enc2(b[1]), enc2(b[2]), enc2(b[3]), enc2(b[4])};
        r.ps = ps; m.push_back(r);
    }
    return m;
}
struct CalResult { double rms, worstErr; double modelWorst[6], measWorst[6]; vector<double> pred; };
static CalResult evaluateCal(const vector<Meas>& M, const Params& p) {
    CalResult r; r.pred.assign(M.size(), 0);
    vector<future<void>> fut;
    for (int d = 0; d < 6; d++) fut.push_back(async(launch::async, [&, d]() {
        Design des(calConfig(d / 2), p, d % 2 ? SZ_STACK : SZ_MIN);
        Sim s(des);
        for (size_t i = 0; i < M.size(); i++) if (M[i].design == d) { int ln; r.pred[i] = s.transition(M[i].a, M[i].b, ln); }
    }));
    for (auto& f : fut) f.get();
    double se = 0; int n = 0;
    for (int d = 0; d < 6; d++) r.modelWorst[d] = r.measWorst[d] = 0;
    for (size_t i = 0; i < M.size(); i++) {
        double e = (r.pred[i] - M[i].ps) / M[i].ps; se += e * e; n++;
        r.modelWorst[M[i].design] = max(r.modelWorst[M[i].design], r.pred[i]);
        r.measWorst[M[i].design] = max(r.measWorst[M[i].design], M[i].ps);
    }
    r.rms = sqrt(se / n);
    r.worstErr = 0;
    for (int d = 0; d < 6; d++) r.worstErr = max(r.worstErr, fabs(r.modelWorst[d] / r.measWorst[d] - 1));
    return r;
}
// Fitted parameters, in log space: mu, gamma, gint, cload, ks (tau has a closed form).
static double* pref(Params& p, int i) { double* r[] = {&p.mu, &p.gamma, &p.gint, &p.cload, &p.ks}; return r[i]; }
static vector<double> getP(Params p, const vector<int>& fr) { vector<double> x; for (int i : fr) x.push_back(log(*pref(p, i))); return x; }
static Params setP(Params p, const vector<int>& fr, const vector<double>& x) { for (size_t k = 0; k < fr.size(); k++) *pref(p, fr[k]) = exp(x[k]); if (p.ks > 1) p.ks = 1; return p; }
// Objective: per-transition relative RMS error, plus the worst-case delay error of each circuit
// (the number the grade uses) so the fit cannot trade the slowest cases away.
static double objective(const vector<Meas>& M, const Params& p) { auto r = evaluateCal(M, p); return r.rms + r.worstErr; }
static Params calibrate(const vector<Meas>& M, Params p0, const vector<int>& fr) {
    // tau has a closed form given the other parameters (least squares in relative error)
    auto fitTau = [&](Params p) {
        p.tau = 1; auto r = evaluateCal(M, p); double num = 0, den = 0;
        for (size_t i = 0; i < M.size(); i++) { double q = r.pred[i] / M[i].ps; num += q; den += q * q; }
        p.tau = num / den; return p;
    };
    int n = fr.size(); vector<vector<double>> S; vector<double> F;
    auto f = [&](vector<double> x) { Params q = fitTau(setP(p0, fr, x)); return objective(M, q); };
    vector<double> x0 = getP(p0, fr);
    S.push_back(x0); F.push_back(f(x0));
    for (int i = 0; i < n; i++) { auto x = x0; x[i] += 0.4; S.push_back(x); F.push_back(f(x)); }
    for (int it = 0; it < 400; it++) {
        vector<int> o(n + 1); iota(o.begin(), o.end(), 0);
        sort(o.begin(), o.end(), [&](int a, int b) { return F[a] < F[b]; });
        vector<vector<double>> S2; vector<double> F2; for (int i : o) { S2.push_back(S[i]); F2.push_back(F[i]); } S = S2; F = F2;
        if (F[n] - F[0] < 1e-5) break;
        vector<double> c(n, 0); for (int i = 0; i < n; i++) for (int k = 0; k < n; k++) c[k] += S[i][k] / n;
        auto mix = [&](double t) { vector<double> x(n); for (int k = 0; k < n; k++) x[k] = c[k] + t * (S[n][k] - c[k]); return x; };
        auto xr = mix(-1); double fr = f(xr);
        if (fr < F[0]) { auto xe = mix(-2); double fe = f(xe); if (fe < fr) { S[n] = xe; F[n] = fe; } else { S[n] = xr; F[n] = fr; } }
        else if (fr < F[n - 1]) { S[n] = xr; F[n] = fr; }
        else {
            auto xc = mix(fr < F[n] ? -0.5 : 0.5); double fc = f(xc);
            if (fc < min(fr, F[n])) { S[n] = xc; F[n] = fc; }
            else for (int i = 1; i <= n; i++) { for (int k = 0; k < n; k++) S[i][k] = S[0][k] + 0.5 * (S[i][k] - S[0][k]); F[i] = f(S[i]); }
        }
    }
    int b = min_element(F.begin(), F.end()) - F.begin();
    return fitTau(setP(p0, fr, S[b]));
}
static void calReport(const vector<Meas>& M, const Params& p, const char* title) {
    auto r = evaluateCal(M, p);
    printf("%s\n  tau=%.3f ps  mu=%.3f  gamma=%.3f  gint=%.3f  cload=%.2f C_fin  ks=%.3f\n", title, p.tau, p.mu, p.gamma, p.gint, p.cload, p.ks);
    printf("  per-transition RMS error %.1f%%, worst-case delay error up to %.1f%%\n", 100 * r.rms, 100 * r.worstErr);
    const char* nm[6] = {"spec_xor_min", "spec_xor_stack", "csa_xor_min", "csa_xor_stack", "csa_mirror_min", "csa_mirror_stack"};
    printf("  %-18s %10s %10s %7s\n", "circuit", "Spectre", "model", "error");
    for (int d = 0; d < 6; d++) printf("  %-18s %8.1f ps %8.1f ps %+6.1f%%\n", nm[d], r.measWorst[d], r.modelWorst[d], 100 * (r.modelWorst[d] / r.measWorst[d] - 1));
    vector<int> om(6), oo(6); iota(om.begin(), om.end(), 0); iota(oo.begin(), oo.end(), 0);
    sort(om.begin(), om.end(), [&](int a, int b) { return r.measWorst[a] < r.measWorst[b]; });
    sort(oo.begin(), oo.end(), [&](int a, int b) { return r.modelWorst[a] < r.modelWorst[b]; });
    // does the model find the slow transitions? rank of Spectre's slowest in the model's order
    printf("  %-18s %s\n", "circuit", "Spectre's slowest transition ranks #k of 816 in the model; model's slowest ranks #k in Spectre; rank correlation");
    for (int d = 0; d < 6; d++) {
        vector<int> idx; for (size_t i = 0; i < M.size(); i++) if (M[i].design == d) idx.push_back(i);
        auto rankBy = [&](auto key) { vector<int> o = idx; sort(o.begin(), o.end(), [&](int x, int y) { return key(x) > key(y); }); vector<double> rk(M.size()); for (size_t k = 0; k < o.size(); k++) rk[o[k]] = k; return rk; };
        auto rm = rankBy([&](int i) { return r.pred[i]; }), rs = rankBy([&](int i) { return M[i].ps; });
        int sw = *max_element(idx.begin(), idx.end(), [&](int x, int y) { return M[x].ps < M[y].ps; });
        int mw = *max_element(idx.begin(), idx.end(), [&](int x, int y) { return r.pred[x] < r.pred[y]; });
        double n = idx.size(), dd = 0; for (int i : idx) dd += (rm[i] - rs[i]) * (rm[i] - rs[i]);
        printf("  %-18s #%-4.0f #%-4.0f  rho=%.2f\n", nm[d], rm[sw] + 1, rs[mw] + 1, 1 - 6 * dd / (n * (n * n - 1)));
    }
    printf("  ranking fastest->slowest  Spectre:"); for (int d : om) printf(" %s", nm[d]);
    printf("\n                           model:  "); for (int d : oo) printf(" %s", nm[d]);
    printf("\n");
}

// ------------------------------------------------------- sizing optimizer
// Greedy in whole fins: add 1 to the fin multiplier of the critical-path gate with the best
// delay gain per added cell area; stop when no move helps. Starts from min sizing.
struct SizePoint { double area, fins, worst; vector<int> mult; };
static vector<SizePoint> optimizeSizing(Design& d, int maxIter = 80) {
    vector<SizePoint> curve;
    auto snap = [&]() { vector<int> s; for (auto& g : d.c.gates) s.push_back(g.mult); return s; };
    Timing full = timeIt(d, nullptr, 200);
    curve.push_back({d.c.area_um2(), d.c.fins(), full.worst, snap()});
    for (int it = 0; it < maxIter; it++) {
        set<int> cand;
        for (auto& [net, t] : full.path) if (d.c.driver[net] >= 0) cand.insert(d.c.driver[net]);
        double A0 = d.c.area_um2(), F0 = d.c.fins(), base = full.worst;
        int bestG = -1; double bestGain = 0;
        for (int g : cand) {
            int old = d.c.gates[g].mult;
            d.c.setMult(g, old + 1); d.c.finalize();
            Timing t = timeIt(d, &full.topPairs, 0);
            double dA = d.c.area_um2() - A0, dF = d.c.fins() - F0;
            double gain = (base - t.worst) / max(dA, 1e-4 * dF);  // a fin that adds no column still costs a little
            if (gain > bestGain) { bestGain = gain; bestG = g; }
            d.c.setMult(g, old); d.c.finalize();
        }
        if (bestG < 0) break;
        int old = d.c.gates[bestG].mult;
        d.c.setMult(bestG, old + 1); d.c.finalize();
        Timing nf = timeIt(d, nullptr, 200);
        if (nf.worst >= base * 0.999) { d.c.setMult(bestG, old); d.c.finalize(); break; }
        full = nf;
        curve.push_back({d.c.area_um2(), d.c.fins(), full.worst, snap()});
    }
    return curve;
}
static void applyMults(Design& d, const vector<int>& m) {
    for (size_t i = 0; i < m.size(); i++) d.c.setMult(i, m[i]);
    d.c.finalize();
}

// ---------------------------------------------------------------- JSON
static string jesc(const string& x) { string o; for (char ch : x) { if (ch == '"' || ch == '\\') o += '\\'; o += ch; } return o; }
static string nodeJson(const Node& n) {
    char b[64];
    if (n.k == Node::LEAF) { snprintf(b, sizeof b, "{\"k\":\"L\",\"pin\":%d,\"w\":%.4g}", n.pin, n.w); return b; }
    string o = string("{\"k\":\"") + (n.k == Node::SER ? "S" : "P") + "\",\"ch\":[";
    for (size_t i = 0; i < n.ch.size(); i++) { if (i) o += ","; o += nodeJson(n.ch[i]); }
    return o + "]}";
}
static string vecJson(int i) {
    if (i < 0) return "null";
    const Vec& v = VECS[i]; char b[96];
    snprintf(b, sizeof b, "{\"x1\":%d,\"x2\":%d,\"w1\":%d,\"w2\":%d,\"wb\":%d}", dec2(v.x1), dec2(v.x2), dec2(v.w1), dec2(v.w2), dec2(v.wb));
    return b;
}
static string designJson(Design& d, const Timing& T, const string& sizing) {
    const Circuit& c = d.c; ostringstream o; o.precision(5);
    o << "{\"arch\":\"" << AN[d.cf.a] << "\",\"ai\":" << d.cf.a << ",\"xor\":\"" << XN[d.cf.x] << "\",\"xi\":" << d.cf.x
      << ",\"fa\":\"" << FN[d.cf.fa] << "\",\"fi\":" << d.cf.fa << ",\"relu\":\"" << RN[d.cf.r] << "\",\"ri\":" << d.cf.r
      << ",\"sizing\":\"" << sizing << "\",\"trans\":" << c.transistors() << ",\"fins\":" << c.fins() << ",\"area\":" << c.area_um2()
      << ",\"worst\":" << T.worst << ",\"rise\":" << T.rise << ",\"fall\":" << T.fall
      << ",\"worstFrom\":" << vecJson(T.wi) << ",\"worstTo\":" << vecJson(T.wj) << ",\"blocks\":[";
    for (size_t i = 0; i < c.blocks.size(); i++) { o << (i ? "," : "") << "["; for (size_t k = 0; k < c.blocks[i].size(); k++) o << (k ? "," : "") << "\"" << c.blocks[i][k] << "\""; o << "]"; }
    o << "],\"path\":[";
    for (size_t i = 0; i < T.path.size(); i++) o << (i ? "," : "") << "[" << T.path[i].first << "," << T.path[i].second << "]";
    o << "],\"nets\":[";
    for (size_t i = 0; i < c.netName.size(); i++) {
        auto it = c.alias.find(i);
        o << (i ? "," : "") << "{\"name\":\"" << jesc(c.netName[i]) << "\",\"alias\":\"" << (it != c.alias.end() ? jesc(it->second) : "")
          << "\",\"drv\":" << c.driver[i] << ",\"C\":" << c.netC[i] << "}";
    }
    o << "],\"inputs\":["; for (size_t i = 0; i < c.inputs.size(); i++) o << (i ? "," : "") << c.inputs[i];
    o << "],\"outputs\":["; for (size_t i = 0; i < c.outputs.size(); i++) o << (i ? "," : "") << c.outputs[i];
    o << "],\"gates\":[";
    for (size_t i = 0; i < c.gates.size(); i++) {
        const Gate& g = c.gates[i];
        o << (i ? "," : "") << "{\"label\":\"" << jesc(g.label) << "\",\"type\":\"" << g.prim->name << "\",\"out\":" << g.out
          << ",\"ndev\":" << g.ndev << ",\"size\":" << g.mult << ",\"in\":[";
        for (size_t k = 0; k < g.in.size(); k++) o << (k ? "," : "") << g.in[k];
        o << "],\"pins\":[";
        for (size_t k = 0; k < g.prim->pins.size(); k++) o << (k ? "," : "") << "\"" << g.prim->pins[k] << "\"";
        o << "],\"pdn\":" << nodeJson(g.pdn) << ",\"pun\":" << nodeJson(g.pun) << "}";
    }
    o << "]}";
    return o.str();
}

// ------------------------------------------------------------ experiments
struct Pt { double area, fins, worst; int trans, row; };
struct Explored { Config cf; Pt u[4], st, bal, fast; vector<string> json; };
static vector<Config> allConfigs() {
    vector<Config> v;
    for (XorStyle x : {X_NAND4, X_CMOS12, X_AOI10}) for (FaStyle fa : {FA_XOR, FA_MIRROR}) {
        for (ReluStyle r : {R_MUX, R_AND}) v.push_back({A_SPEC, x, fa, r, M_FIXED});
        v.push_back({A_CSA, x, fa, R_NONE, M_FIXED});
    }
    return v;
}
static Explored explore(Config cf, Params p, bool json) {
    Explored e; e.cf = cf;
    auto pt = [](Design& d, double T) { return Pt{d.c.area_um2(), d.c.fins(), T, d.c.transistors(), d.c.row()}; };
    Design ds(cf, p, SZ_STACK);
    if (funcErrors(ds)) { fprintf(stderr, "functional error in %s\n", AN[cf.a]); exit(1); }
    Timing ts = timeIt(ds, nullptr, 0); e.st = pt(ds, ts.worst);
    Timing t1; vector<int> bestM, fastM; double bestTA = INF, fastT = INF;
    for (int f = 1; f <= 3; f++) {
        Design d(cf, p, SZ_MIN);
        for (size_t i = 0; i < d.c.gates.size(); i++) d.c.setMult(i, f);
        d.c.finalize();
        Timing t = timeIt(d, nullptr, 0); e.u[f] = pt(d, t.worst);
        if (f == 1) { t1 = t; if (json) e.json.push_back(designJson(d, t, "min")); }
        for (auto& sp : optimizeSizing(d)) {
            if (sp.worst * sp.area < bestTA) { bestTA = sp.worst * sp.area; bestM = sp.mult; e.bal = {sp.area, sp.fins, sp.worst, e.st.trans, 0}; }
            if (sp.worst < fastT) { fastT = sp.worst; fastM = sp.mult; e.fast = {sp.area, sp.fins, sp.worst, e.st.trans, 0}; }
        }
    }
    if (ts.worst * e.st.area < bestTA) { e.bal = e.st; bestM.clear(); }
    Design d(cf, p, SZ_MIN);
    if (!bestM.empty()) { applyMults(d, bestM); e.bal.row = d.c.row(); }
    if (json) {
        e.json.push_back(designJson(ds, ts, "stack"));
        if (!bestM.empty()) e.json.push_back(designJson(d, timeIt(d, nullptr, 0), "balanced"));
        else e.json.push_back(designJson(ds, ts, "balanced"));
        applyMults(d, fastM); e.fast.row = d.c.row();
        e.json.push_back(designJson(d, timeIt(d, nullptr, 0), "fastest"));
    }
    return e;
}

int main(int argc, char** argv) {
    Params p; string jsonPath, calPath; bool quick = false;
    for (int i = 1; i < argc; i++) {
        string k = argv[i];
        if (k == "--quick") { quick = true; continue; }
        if (i + 1 >= argc) break;
        string v = argv[++i];
        if (k == "--json") jsonPath = v; else if (k == "--calibrate") calPath = v;
        else if (k == "--tau") p.tau = stod(v); else if (k == "--mu") p.mu = stod(v); else if (k == "--gamma") p.gamma = stod(v);
        else if (k == "--gint") p.gint = stod(v); else if (k == "--cload") p.cload = stod(v); else if (k == "--stack_beta") p.stack_beta = stod(v);
    }
    if (!calPath.empty()) {
        auto M = loadMeas(calPath);
        printf("Calibration data: %zu Spectre transitions from %s\n\n", M.size(), calPath.c_str());
        Params p0 = p; p0.gamma = 1; p0.gint = 0.5; p0.cload = 20; p0.mu = 1.2; p0.ks = 1;
        Params pold = p0; pold.mu = 2.0; pold.gint = 1e-6; pold.tau = 1; pold.ks = 1;  // the previous model's assumptions
        {   // old model: stack nodes ignored, mu = 2; tau and cload refit so only the physics differs
            auto fitOld = [&](Params q) { q.tau = 1; auto r = evaluateCal(M, q); double a = 0, b = 0; for (size_t i = 0; i < M.size(); i++) { double z = r.pred[i] / M[i].ps; a += z; b += z * z; } q.tau = a / b; return q; };
            pold = fitOld(pold);
            calReport(M, pold, "Previous assumptions (mu=2, no stack-node cap, tau fitted)");
        }
        Params pa = calibrate(M, p0, {0, 1, 2, 3});
        printf("\n"); calReport(M, pa, "Fit A: stack-node cap, linear stacks, mu free");
        Params pb = calibrate(M, p0, {1, 2, 3, 4});
        printf("\n"); calReport(M, pb, "Fit B: stack-node cap, stack factor ks, mu fixed at the measured 1.20");
        Params pc = calibrate(M, pb, {0, 1, 2, 3, 4});
        printf("\n"); calReport(M, pc, "Fit C: everything free");
        return 0;
    }
    if (quick) {
        for (auto& cf : allConfigs()) for (Sizing s : {SZ_MIN, SZ_STACK}) {
            Design d(cf, p, s); Timing t = timeIt(d, nullptr, 0);
            printf("%-17s %-14s %-13s %-24s %-5s T=%6.1f ps fins=%4.0f A=%6.3f um2 TA=%7.2f\n", AN[cf.a], XN[cf.x], FN[cf.fa], RN[cf.r], SZN[s], t.worst, d.c.fins(), d.c.area_um2(), t.worst * d.c.area_um2());
        }
        return 0;
    }

    printf("Model: ASAP7-calibrated  tau=%.3f ps  mu=%.2f  gamma=%.2f  gint=%.2f  cload=%.1f C_fin  stack_beta=%.2f\n", p.tau, p.mu, p.gamma, p.gint, p.cload, p.stack_beta);
    printf("Delays in ps (pre-layout, TT 25 C 0.9 V). Cell area: best uniform row of 1-3 fins, (2r+4)x27 nm tall, 54 nm CPP, no routing.\n");
    printf("%zu output-changing transitions per design (weights held fixed).\n\n", PAIRS.size());

    printf("=== Functional check, 243 valid combinations ===\n");
    for (Config cf : {Config{A_SPEC, X_CMOS12, FA_XOR, R_AND, M_SCREENSHOT}, Config{A_SPEC, X_CMOS12, FA_XOR, R_AND, M_FIXED}, Config{A_CSA, X_CMOS12, FA_XOR, R_NONE, M_FIXED}}) {
        Design d(cf, p, SZ_MIN); vector<Vec> ex; int e = funcErrors(d, &ex);
        printf("%-17s multiplier=%-10s errors=%d\n", AN[cf.a], MN[cf.m], e);
        for (auto& v : ex) printf("    e.g. %s  expected %d\n", vstr(v).c_str(), reference(v));
    }

    auto cfgs = allConfigs();
    vector<future<Explored>> fut;
    for (auto& cf : cfgs) fut.push_back(async(launch::async, explore, cf, p, !jsonPath.empty()));
    vector<Explored> R; for (auto& f : fut) R.push_back(f.get());

    auto score = [](const Explored& e) { return e.bal.worst * e.bal.area; };
    vector<int> order(R.size()); iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b) { return score(R[a]) < score(R[b]); });
    double refTA = score(R[order[0]]);

    printf("\n=== All designs: worst delay T (ps) / cell-area estimate A (um2); f/A relative to the best ===\n");
    printf("%-17s %-14s %-13s %-23s %5s | %-14s %-14s %-14s | %-14s | %-19s | %5s\n", "Architecture", "XOR", "Full adder", "ReLU", "Trans",
           "1 fin", "2 fins", "3 fins", "stack", "optimized (row)", "f/A");
    for (int i : order) {
        auto& e = R[i];
        printf("%-17s %-14s %-13s %-23s %5d | %5.1f / %5.3f  %5.1f / %5.3f  %5.1f / %5.3f  | %5.1f / %5.3f  | %5.1f / %5.3f (%d) | %5.2f\n",
               AN[e.cf.a], XN[e.cf.x], FN[e.cf.fa], RN[e.cf.r], e.st.trans, e.u[1].worst, e.u[1].area, e.u[2].worst, e.u[2].area, e.u[3].worst, e.u[3].area,
               e.st.worst, e.st.area, e.bal.worst, e.bal.area, e.bal.row, refTA / score(e));
    }

    for (Arch a : {A_SPEC, A_CSA}) {
        int bi = -1; for (int i : order) if (R[i].cf.a == a) { bi = i; break; }
        auto& e = R[bi];
        Design d(e.cf, p, SZ_MIN); Timing t = timeIt(d, nullptr, 0);
        printf("\nBest %s: %s / %s / %s\n", AN[a], XN[e.cf.x], FN[e.cf.fa], RN[e.cf.r]);
        printf("  worst-case testbench vector (min sizing): [%s] -> [%s]  %.1f ps\n", vstr(VECS[t.wi]).c_str(), vstr(VECS[t.wj]).c_str(), t.worst);
        printf("  critical path:");
        for (auto& [net, tt] : t.path) printf(" %s", d.c.driver[net] >= 0 ? d.c.gates[d.c.driver[net]].label.c_str() : d.c.netName[net].c_str());
        printf("\n");
    }

    printf("\n=== Stack sizing P/N ratio sweep (best design of each architecture) ===\n%10s", "stack_beta");
    vector<int> best2;
    for (Arch a : {A_SPEC, A_CSA}) for (int i : order) if (R[i].cf.a == a) { best2.push_back(i); printf(" %24s", AN[a]); break; }
    printf("\n");
    string betaJson = "[";
    bool firstB = true;
    for (double beta : {1.0, 1.2, 1.5, 2.0}) {
        printf("%10.2f", beta);
        betaJson += string(firstB ? "" : ",") + "{\"beta\":" + to_string(beta); firstB = false;
        for (int k = 0; k < 2; k++) {
            Params q = p; q.stack_beta = beta;
            Design d(R[best2[k]].cf, q, SZ_STACK); Timing t = timeIt(d, nullptr, 0);
            printf(" %9.1f ps %7.3f um2  ", t.worst, d.c.area_um2());
            betaJson += ",\"" + string(k ? "csa" : "spec") + "\":" + to_string(t.worst) + ",\"" + string(k ? "csaA" : "specA") + "\":" + to_string(d.c.area_um2());
        }
        betaJson += "}"; printf("\n");
    }
    betaJson += "]";

    if (!jsonPath.empty()) {
        FILE* f = fopen(jsonPath.c_str(), "w");
        fprintf(f, "{\"params\":{\"tau\":%g,\"mu\":%g,\"gamma\":%g,\"gint\":%g,\"cload\":%g,\"stack_beta\":%g},\"beta\":%s,\"designs\":[", p.tau, p.mu, p.gamma, p.gint, p.cload, p.stack_beta, betaJson.c_str());
        bool first = true;
        for (auto& e : R) for (auto& j : e.json) { fprintf(f, "%s%s", first ? "" : ",", j.c_str()); first = false; }
        fprintf(f, "]}\n"); fclose(f);
        printf("\nwrote %s\n", jsonPath.c_str());
    }
    return 0;
}
