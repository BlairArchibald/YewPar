#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <limits>

#include "../maxclique/DimacsParser.hpp"
#include "../maxclique/BitGraph.hpp"
#include "../maxclique/BitSet.hpp"

#ifndef NWORDS
#define NWORDS 16
#endif

// Build complement graph of DIMACS .clq input
template<unsigned n_words_>
BitGraph<n_words_> buildGraphFromFile(const dimacs::GraphFromFile &g) {
    int N = g.first;

    BitGraph<n_words_> original;
    original.resize(N);

    for (auto &kv : g.second) {
        int u = kv.first;
        for (int v : kv.second) {
            if (u != v) original.add_edge(u, v);
        }
    }

    BitGraph<n_words_> comp;
    comp.resize(N);

    for (int u = 0; u < N; ++u) {
        for (int v = u + 1; v < N; ++v) {
            if (!original.adjacent(u, v)) {
                comp.add_edge(u, v);
            }
        }
    }
    return comp;
}

// Vertex Cover node
struct VCSol {
    std::vector<int> cover;
};

struct VCNode {
    VCSol sol;
    int size;
    BitSet<NWORDS> inCover;
    BitSet<NWORDS> active;
    bool isCover;

    int getObj() const {
        if (!isCover) return std::numeric_limits<int>::max();
        return size;
    }
};

// Check uncovered edges
bool check_is_cover(const BitGraph<NWORDS> &g, const VCNode &n) {
    int N = g.size();
    BitSet<NWORDS> rest = n.active;
    for (int i = 0; i < N; ++i) {
        if (n.inCover.test(i)) {
            rest.unset(i);
        }
    }

    for (int u = 0; u < N; ++u) {
        if (!rest.test(u)) continue;
        BitSet<NWORDS> nbrs = rest;
        g.intersect_with_row(u, nbrs);
        if (!nbrs.empty()) return false;
    }
    return true;
}

// reduction: degree-0
bool apply_reductions(const BitGraph<NWORDS> &g, VCNode &n) {
    bool changed = false;
    int N = g.size();

    while (true) {
        bool local = false;

        BitSet<NWORDS> undec = n.active;
        for (int i = 0; i < N; ++i) {
            if (n.inCover.test(i)) {
                undec.unset(i);
            }
        }

        for (int u = 0; u < N; ++u) {
            if (!undec.test(u)) continue;

            BitSet<NWORDS> nbrs = n.active;
            g.intersect_with_row(u, nbrs);

            for (int v = 0; v < N; ++v) {
                if (nbrs.test(v) && n.inCover.test(v)) {
                    nbrs.unset(v);
                }
            }

            if (nbrs.empty()) {
                n.active.unset(u);
                undec.unset(u);
                local = true;
            }
        }
        changed |= local;
        if (!local) break;
    }
    n.isCover = check_is_cover(g, n);
    return changed;
}

// Greedy maximal matching lower bound
int matchingLB(const BitGraph<NWORDS> &g, const VCNode &n) {
    int N = g.size();
    BitSet<NWORDS> avail = n.active;

    for (int i = 0; i < N; ++i) {
        if (n.inCover.test(i)) {
            avail.unset(i);
        }
    }

    std::vector<bool> used(N, false);
    int M = 0;

    for (int u = 0; u < N; ++u) {
        if (!avail.test(u) || used[u]) continue;

        BitSet<NWORDS> nbrs = avail;
        g.intersect_with_row(u, nbrs);

        for (int v = 0; v < N; ++v) {
            if (nbrs.test(v) && used[v]) {
                nbrs.unset(v);
            }
        }

        int v = nbrs.first_set_bit();
        if (v != -1) {
            used[u] = used[v] = true;
            avail.unset(u);
            avail.unset(v);
            M++;
        } 
        else {
            avail.unset(u);
        }
    }
    return n.size + M;
}

// Branching helper: pick vertex with maximum residual degree
int pick_branch_vertex(const BitGraph<NWORDS> &g, const VCNode &n) {
    int N = g.size();
    int best = -1, bestD = -1;

    for (int u = 0; u < N; ++u) {
        if (!n.active.test(u) || n.inCover.test(u)) continue;

        BitSet<NWORDS> nbrs = n.active;
        g.intersect_with_row(u, nbrs);

        for (int v = 0; v < N; ++v) {
            if (nbrs.test(v) && n.inCover.test(v)) {
                nbrs.unset(v);
            }
        }
        int d = nbrs.popcount();
        if (d > bestD) {
            bestD = d;
            best = u;
        }
    }
    return best;
}

// Recursive branch-and-bound
VCNode bestSol;

void dfs(const BitGraph<NWORDS> &g, VCNode node, int UB) {

    int LB = matchingLB(g, node);
    if (LB >= UB) return;

    if (node.isCover) {
        if (node.size < bestSol.size)
            bestSol = node;
        return;
    }

    int v = pick_branch_vertex(g, node);
    if (v == -1) return;

    // include v
    {
        VCNode c = node;
        if (!c.inCover.test(v)) {
            c.inCover.set(v);
            c.sol.cover.push_back(v);
            c.size++;
        }
        apply_reductions(g, c);
        dfs(g, c, UB);
        UB = bestSol.size;
    }

    // exclude v
    {
        VCNode c = node;
        int N = g.size();

        BitSet<NWORDS> nbrs = c.active;
        g.intersect_with_row(v, nbrs);

        for (int u = 0; u < N; ++u) {
            if (nbrs.test(u) && !c.inCover.test(u)) {
                c.inCover.set(u);
                c.sol.cover.push_back(u);
                c.size++;
            }
        }
        
        c.active.unset(v);
        apply_reductions(g, c);
        dfs(g, c, UB);
    }
}

int main(int argc, char** argv) {

    if (argc < 3 || std::string(argv[1]) != "--input-file") {
        std::cout << "Usage: ./vc_seq --input-file file.clq\n";
        return 0;
    }

    auto gFile = dimacs::read_dimacs(argv[2]);
    auto graph = buildGraphFromFile<NWORDS>(gFile);

    VCNode root;
    root.size = 0;
    root.inCover.resize(graph.size());
    root.inCover.reset_all();
    root.active.resize(graph.size());
    root.active.set_all();
    root.isCover = check_is_cover(graph, root);

    apply_reductions(graph, root);

    bestSol = root;
    bestSol.size = graph.size(); // worst-case upper bound

    auto t0 = std::chrono::steady_clock::now();
    dfs(graph, root, bestSol.size);
    auto t1 = std::chrono::steady_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    std::cout << "Minimum Vertex Cover Size = " << bestSol.size << "\n";
    std::cout << "cpu = " << ms << " ms\n";
}
