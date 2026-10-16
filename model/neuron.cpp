// ECE555 neuron: whole-design model with switch-level CMOS timing.
//
//   out = ReLU(x1*w1 + x2*w2 + wb)        2-bit codes: 00=0, 01=+1, 11=-1 (10 unused)
//
// Every gate is a static CMOS gate described by its pull-down network (PDN) as a
// series/parallel tree; the pull-up (PUN) is its dual. Devices are sized so the worst
// stack equals a unit inverter (NMOS 1, PMOS beta, in Wunit), times a per-gate size.
//
// Timing, in tau = R_unit * C_unit of one minimum NMOS:
//   R(on network) from series/parallel ON devices; NMOS w -> 1/w, PMOS w -> mu/w
//   C(net) = fanout gate cap + gamma * output diffusion (+ cload on outputs)
//   delay  = ln2 * R * C, event driven, inertial
//   primary inputs are driven by a unit inverter (rdrv), so input cap costs time
// Ignores stack-node cap, slew and wires: use it to rank designs and pick vectors.
//
// What it explores:
//   architecture  SPEC : MUL -> 2-bit ADD -> 3-bit ADD (+bias) -> ReLU   (slide 14 blocks)
//                 CSA  : each product is -1/0/+1 = (nonzero, negative) bit pair, so
//                        count nonzeros and negatives with two parallel full adders and
//                        compute ReLU(count difference) directly with two gates
//   cells         XOR style (16T NAND, 12T CMOS, 10T NOR+AOI), full adder (XOR or mirror),
//                 ReLU (3-bit mux or AND gating)
//   sizing        greedy critical-path upsizing (TILOS-style), area = sum of device widths
//   beta          global P/N ratio sweep
//
// Usage: ./neuron                 text report
//        ./neuron --json out.json  also export every netlist for the diagram page
//        --beta --mu --gamma --cload --rdrv  override model parameters

#include <bits/stdc++.h>
#include <future>
using namespace std;

struct Params {
    double beta = 1.5, mu = 2.0, gamma = 1.0, cload = 6.0, rdrv = 1.0;
};

// ---------------------------------------------------------------- SP networks
struct Node {
    enum K { LEAF, SER, PAR } k;
    int pin = -1;
    double w = 0;
    vector<Node> ch;
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
static void size_net(Node& n, double R, double scale) {
    if (n.k == Node::LEAF) { n.w = scale / R; return; }
    int tot = slen(n);
    for (auto& c : n.ch) size_net(c, n.k == Node::SER ? R * slen(c) / tot : R, scale);
}
static int count_dev(const Node& n) { if (n.k == Node::LEAF) return 1; int r = 0; for (auto& c : n.ch) r += count_dev(c); return r; }
static double sum_w(const Node& n) { if (n.k == Node::LEAF) return n.w; double r = 0; for (auto& c : n.ch) r += sum_w(c); return r; }
static void pin_widths(const Node& n, vector<double>& acc) { if (n.k == Node::LEAF) { acc[n.pin] += n.w; return; } for (auto& c : n.ch) pin_widths(c, acc); }
static double top_width(const Node& n) {
    if (n.k == Node::LEAF) return n.w;
    if (n.k == Node::SER) return top_width(n.ch[0]);
    double r = 0; for (auto& c : n.ch) r += top_width(c); return r;
}
static const double INF = 1e18;
static double resist(const Node& n, const int* v, bool pmos, double mu) {
    if (n.k == Node::LEAF) { bool on = pmos ? !v[n.pin] : v[n.pin]; return on ? (pmos ? mu : 1.0) / n.w : INF; }
    if (n.k == Node::SER) { double r = 0; for (auto& c : n.ch) { double x = resist(c, v, pmos, mu); if (x >= INF) return INF; r += x; } return r; }
    double g = 0; for (auto& c : n.ch) { double x = resist(c, v, pmos, mu); if (x < INF) g += 1 / x; }
    return g > 0 ? 1 / g : INF;
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
    const Prim* prim; vector<int> in; int out; string label; double size = 1;
    Node pdn, pun; int ndev; vector<double> pinC; double diff, area;
};

struct Circuit {
    Params p;
    vector<string> netName; vector<int> driver; vector<vector<int>> fanout;
    vector<Gate> gates; vector<int> inputs, outputs;
    int GND, VDD; string prefix;
    map<int, int> invCache; map<tuple<int, int, int>, int> xorCache; map<int, string> alias;
    vector<vector<string>> blocks;
    vector<double> netC;

    Circuit(Params pp) : p(pp) { GND = net("GND"); VDD = net("VDD"); }
    int net(const string& n) { netName.push_back(n); driver.push_back(-1); fanout.emplace_back(); return (int)netName.size() - 1; }
    int input(const string& n) { int x = net(n); inputs.push_back(x); return x; }
    void applySize(Gate& g) {
        g.pdn = g.prim->pdn; g.pun = dual(g.prim->pdn);
        size_net(g.pdn, 1.0, g.size); size_net(g.pun, 1.0, g.size * p.beta);
        g.ndev = count_dev(g.pdn) + count_dev(g.pun);
        g.pinC.assign(g.prim->npins, 0); pin_widths(g.pdn, g.pinC); pin_widths(g.pun, g.pinC);
        g.diff = top_width(g.pdn) + top_width(g.pun);
        g.area = sum_w(g.pdn) + sum_w(g.pun);
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
        for (int o : outputs) netC[o] += p.cload;
    }
    void resize(int gi, double s) { gates[gi].size = s; applySize(gates[gi]); }
    int transistors() const { int n = 0; for (auto& g : gates) n += g.ndev; return n; }
    double area() const { double a = 0; for (auto& g : gates) a += g.area; return a; }
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
static vector<pair<int, int>> makePairs() {
    vector<pair<int, int>> p;
    for (int i = 0; i < (int)VECS.size(); i++) for (int j = 0; j < (int)VECS.size(); j++)
        if (i != j && reference(VECS[i]) != reference(VECS[j])) p.push_back({i, j});
    return p;
}
static const vector<pair<int, int>> PAIRS = makePairs();

struct Design {  // a built circuit plus its pins
    Circuit c; int in[10]; vector<int> o; Config cf;
    Design(Config cf_, Params p) : c(p), cf(cf_) {
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
    int evalGate(const Gate& g) { loadIn(g); return resist(g.pdn, gin, false, 1) >= INF ? 1 : 0; }
    double delay(const Gate& g, int nv) {
        loadIn(g);
        double R = nv ? resist(g.pun, gin, true, c.p.mu) : resist(g.pdn, gin, false, 1);
        return M_LN2 * R * c.netC[g.out];
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
        int target[10];
        for (int k = 0; k < 10; k++) {
            target[k] = (codes[k / 2] >> (k % 2)) & 1;
            int n = in[k];
            if (target[k] != val[n]) {
                double R = target[k] ? c.p.mu / c.p.beta : 1.0;  // unit-inverter driver
                q.push({M_LN2 * c.p.rdrv * R * c.netC[n], 0, -(k + 1)});
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
        return last;
    }
    vector<pair<int, double>> tracePath(int net) {
        vector<pair<int, double>> p;
        while (net >= 0) { p.push_back({net, tChange[net]}); net = tCause[net]; }
        reverse(p.begin(), p.end()); return p;
    }
};

struct Timing {
    double worst = 0, rise = 0, fall = 0, wfix = 0;
    int wi = -1, wj = -1, fi = -1, fj = -1;
    vector<pair<int, double>> path;
    vector<int> topPairs;  // indices into PAIRS of the slowest transitions
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
        const Vec &a = VECS[i], &b = VECS[j];
        if (a.w1 == b.w1 && a.w2 == b.w2 && a.wb == b.wb && t > T.wfix) { T.wfix = t; T.fi = i; T.fj = j; }
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
static double fo4(const Params& p) {
    double cin = 1 + p.beta, cl = p.gamma * cin + 4 * cin;
    return (M_LN2 * cl + M_LN2 * (p.mu / p.beta) * cl) / 2;
}

// ------------------------------------------------------- sizing optimizer
// Greedy: repeatedly upsize (x1.25) the critical-path gate with the best
// delay gain per unit of added area, checking candidates on the slowest
// transitions and re-timing all transitions after each accepted move.
struct SizePoint { double area, worst; vector<double> sizes; };
static vector<SizePoint> optimizeSizing(Design& d, int maxIter = 80) {
    vector<SizePoint> curve;
    auto snap = [&]() { vector<double> s; for (auto& g : d.c.gates) s.push_back(g.size); return s; };
    Timing full = timeIt(d, nullptr, 400);
    curve.push_back({d.c.area(), full.worst, snap()});
    for (int it = 0; it < maxIter; it++) {
        set<int> cand;
        for (auto& [net, t] : full.path) if (d.c.driver[net] >= 0) cand.insert(d.c.driver[net]);
        // also the gates driving side inputs of path gates (load reduction is not modeled by upsizing,
        // but upsizing a fanout-heavy driver helps): include drivers of the path gates' inputs
        double A0 = d.c.area(), base = full.worst;
        int bestG = -1; double bestGain = 0;
        for (int g : cand) {
            double old = d.c.gates[g].size;
            d.c.resize(g, old * 1.25); d.c.finalize();
            Timing t = timeIt(d, &full.topPairs, 0);
            double dA = d.c.area() - A0, gain = (base - t.worst) / dA;
            if (gain > bestGain) { bestGain = gain; bestG = g; }
            d.c.resize(g, old); d.c.finalize();
        }
        if (bestG < 0) break;
        double old = d.c.gates[bestG].size;
        d.c.resize(bestG, old * 1.25); d.c.finalize();
        Timing nf = timeIt(d, nullptr, 400);
        if (nf.worst >= base * 0.999) { d.c.resize(bestG, old); d.c.finalize(); break; }
        full = nf;
        curve.push_back({d.c.area(), full.worst, snap()});
    }
    return curve;
}
static void applySizes(Design& d, const vector<double>& s) {
    for (size_t i = 0; i < s.size(); i++) d.c.resize(i, s[i]);
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
      << ",\"sizing\":\"" << sizing << "\",\"trans\":" << c.transistors() << ",\"area\":" << c.area()
      << ",\"worst\":" << T.worst << ",\"fo4\":" << fo4(c.p) << ",\"rise\":" << T.rise << ",\"fall\":" << T.fall
      << ",\"wfix\":" << T.wfix << ",\"wfixFrom\":" << vecJson(T.fi) << ",\"wfixTo\":" << vecJson(T.fj)
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
          << ",\"ndev\":" << g.ndev << ",\"size\":" << g.size << ",\"in\":[";
        for (size_t k = 0; k < g.in.size(); k++) o << (k ? "," : "") << g.in[k];
        o << "],\"pins\":[";
        for (size_t k = 0; k < g.prim->pins.size(); k++) o << (k ? "," : "") << "\"" << g.prim->pins[k] << "\"";
        o << "],\"pdn\":" << nodeJson(g.pdn) << ",\"pun\":" << nodeJson(g.pun) << "}";
    }
    o << "]}";
    return o.str();
}

// ------------------------------------------------------------ experiments
struct Explored {
    Config cf;
    double area0, worst0, areaB, worstB, areaF, worstF; int trans;
    string jUnit, jBal, jFast;
    vector<pair<double, double>> curve;
};
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
    Design d(cf, p);
    if (funcErrors(d)) { fprintf(stderr, "functional error in %s\n", AN[cf.a]); exit(1); }
    Timing t0 = timeIt(d, nullptr, 0);
    e.trans = d.c.transistors(); e.area0 = d.c.area(); e.worst0 = t0.worst;
    if (json) e.jUnit = designJson(d, t0, "unit");
    auto curve = optimizeSizing(d);
    for (auto& pt : curve) e.curve.push_back({pt.area, pt.worst});
    // fastest = last point; balanced = min worst*area
    const SizePoint* bal = &curve[0];
    for (auto& pt : curve) if (pt.worst * pt.area < bal->worst * bal->area) bal = &pt;
    const SizePoint& fast = curve.back();
    e.areaB = bal->area; e.worstB = bal->worst; e.areaF = fast.area; e.worstF = fast.worst;
    if (json) {
        applySizes(d, bal->sizes); e.jBal = designJson(d, timeIt(d, nullptr, 0), "balanced");
        applySizes(d, fast.sizes); e.jFast = designJson(d, timeIt(d, nullptr, 0), "fastest");
    }
    return e;
}

int main(int argc, char** argv) {
    Params p; string jsonPath;
    for (int i = 1; i + 1 < argc; i += 2) {
        string k = argv[i];
        if (k == "--json") { jsonPath = argv[i + 1]; continue; }
        double v = atof(argv[i + 1]);
        if (k == "--beta") p.beta = v; else if (k == "--mu") p.mu = v; else if (k == "--gamma") p.gamma = v;
        else if (k == "--cload") p.cload = v; else if (k == "--rdrv") p.rdrv = v;
    }
    double F = fo4(p);
    printf("Params: beta=%.2f mu=%.2f gamma=%.2f cload=%.1f rdrv=%.1f   1 FO4 = %.1f tau\n", p.beta, p.mu, p.gamma, p.cload, p.rdrv, F);
    printf("Area = sum of device widths in Wunit (proxy for layout area). %zu output-changing transitions per design.\n\n", PAIRS.size());

    printf("=== Functional check, 243 valid combinations ===\n");
    for (Config cf : {Config{A_SPEC, X_CMOS12, FA_XOR, R_AND, M_SCREENSHOT}, Config{A_SPEC, X_CMOS12, FA_XOR, R_AND, M_FIXED}, Config{A_CSA, X_CMOS12, FA_XOR, R_NONE, M_FIXED}}) {
        Design d(cf, p); vector<Vec> ex; int e = funcErrors(d, &ex);
        printf("%-17s multiplier=%-10s errors=%d\n", AN[cf.a], MN[cf.m], e);
        for (auto& v : ex) printf("    e.g. %s  expected %d\n", vstr(v).c_str(), reference(v));
    }

    auto cfgs = allConfigs();
    vector<future<Explored>> fut;
    for (auto& cf : cfgs) fut.push_back(async(launch::async, explore, cf, p, !jsonPath.empty()));
    vector<Explored> R; for (auto& f : fut) R.push_back(f.get());

    printf("\n=== All designs (unit sizing, then after the sizing optimizer) ===\n");
    printf("%-17s %-14s %-13s %-22s %5s | %6s %7s %5s | %6s %7s %5s | %6s %7s %5s\n", "Architecture", "XOR", "Full adder", "ReLU", "Trans",
           "Area", "T", "FO4", "Area", "T", "FO4", "Area", "T", "FO4");
    printf("%-75s | %-20s | %-20s | %-20s\n", "", "unit", "balanced (min T*A)", "fastest");
    vector<int> order(R.size()); iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b) { return R[a].worstB * R[a].areaB < R[b].worstB * R[b].areaB; });
    for (int i : order) {
        auto& e = R[i];
        printf("%-17s %-14s %-13s %-22s %5d | %6.0f %7.1f %5.1f | %6.0f %7.1f %5.1f | %6.0f %7.1f %5.1f\n", AN[e.cf.a], XN[e.cf.x], FN[e.cf.fa], RN[e.cf.r], e.trans,
               e.area0, e.worst0, e.worst0 / F, e.areaB, e.worstB, e.worstB / F, e.areaF, e.worstF, e.worstF / F);
    }

    // detail for best of each architecture
    for (Arch a : {A_SPEC, A_CSA}) {
        int bi = -1;
        for (int i : order) if (R[i].cf.a == a) { bi = i; break; }
        auto& e = R[bi];
        Design d(e.cf, p);
        Timing t = timeIt(d, nullptr, 0);
        printf("\nBest %s: %s / %s / %s\n", AN[a], XN[e.cf.x], FN[e.cf.fa], RN[e.cf.r]);
        printf("  worst-case vector (weights fixed): [%s] -> [%s]  %.1f tau\n", vstr(VECS[t.fi]).c_str(), vstr(VECS[t.fj]).c_str(), t.wfix);
        printf("  critical path (unit sizing):");
        for (auto& [net, tt] : t.path) printf(" %s", d.c.driver[net] >= 0 ? d.c.gates[d.c.driver[net]].label.c_str() : d.c.netName[net].c_str());
        printf("\n  sizing curve (area, T):");
        for (size_t k = 0; k < e.curve.size(); k += max<size_t>(1, e.curve.size() / 8)) printf(" (%.0f, %.1f)", e.curve[k].first, e.curve[k].second);
        printf(" ... (%.0f, %.1f)\n", e.curve.back().first, e.curve.back().second);
    }

    // beta sweep for the best design of each architecture (unit sizing)
    printf("\n=== Beta sweep, unit sizing ===\n%6s", "beta");
    vector<int> best2;
    for (Arch a : {A_SPEC, A_CSA}) for (int i : order) if (R[i].cf.a == a) { best2.push_back(i); printf(" %18s", AN[a]); break; }
    printf("\n");
    string betaJson = "[";
    for (double beta = 1.0; beta <= 3.01; beta += 0.25) {
        printf("%6.2f", beta);
        betaJson += (beta > 1.0 ? "," : "") + string("{\"beta\":") + to_string(beta);
        for (int k = 0; k < 2; k++) {
            Params q = p; q.beta = beta;
            Design d(R[best2[k]].cf, q);
            Timing t = timeIt(d, nullptr, 0);
            printf(" %12.1f tau %4.0fA", t.worst, d.c.area());
            betaJson += ",\"" + string(k ? "csa" : "spec") + "\":" + to_string(t.worst) + ",\"" + string(k ? "csaA" : "specA") + "\":" + to_string(d.c.area());
        }
        betaJson += "}";
        printf("\n");
    }
    betaJson += "]";

    if (!jsonPath.empty()) {
        FILE* f = fopen(jsonPath.c_str(), "w");
        fprintf(f, "{\"params\":{\"beta\":%g,\"mu\":%g,\"gamma\":%g,\"cload\":%g,\"rdrv\":%g},\"beta\":%s,\"designs\":[", p.beta, p.mu, p.gamma, p.cload, p.rdrv, betaJson.c_str());
        bool first = true;
        for (auto& e : R) for (auto* j : {&e.jUnit, &e.jBal, &e.jFast}) { fprintf(f, "%s%s", first ? "" : ",", j->c_str()); first = false; }
        fprintf(f, "],\"curves\":[");
        for (size_t i = 0; i < R.size(); i++) {
            fprintf(f, "%s{\"ai\":%d,\"xi\":%d,\"fi\":%d,\"ri\":%d,\"pts\":[", i ? "," : "", R[i].cf.a, R[i].cf.x, R[i].cf.fa, R[i].cf.r);
            for (size_t k = 0; k < R[i].curve.size(); k++) fprintf(f, "%s[%.2f,%.3f]", k ? "," : "", R[i].curve[k].first, R[i].curve[k].second);
            fprintf(f, "]}");
        }
        fprintf(f, "]}\n"); fclose(f);
        printf("\nwrote %s\n", jsonPath.c_str());
    }
    return 0;
}
