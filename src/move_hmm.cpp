#include <vector>
#include <string>

#include <cstddef>

#include "fast5/src/fast5.hpp"

#include "move_hmm.h"
#include "kmers.h"
#include "log2_num.h"

#define DBG(M, ...) \
  fprintf(stderr, "%s:%d: " M "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)

const int kInitialState = 0;

std::vector<State<double>*> constructEmissions(
    size_t k, const std::vector<GaussianParamsKmer>& kmer_gaussians) {
  size_t num_kmers = numKmersOf(k);
  assert(num_kmers == kmer_gaussians.size());
  std::vector<State<double>*> res(num_kmers + 1);
  res[kInitialState] = new SilentState<double>();
  for (const GaussianParamsKmer& gaussian : kmer_gaussians) {
    assert(gaussian.kmer_.size() == k);
    int state = kmerToLexicographicPos(gaussian.kmer_);
    res[state] = new GaussianState(gaussian.mu_, gaussian.sigma_);
  }

  return res;
}

std::vector<std::vector<Transition>> constructTransitions(int move_threshold,
                                                          int pseudo_count,
                                                          int k,
                                                          Fast5Reads* reads) {
  int states = numKmersOf(k) + 1;

  // Count for every transition how many times it occurred for every given
  // sequence.
  std::map<std::pair<int, int>, int> count_for_transition;
  while (reads->hasNextRead()) {
    const std::vector<MoveKmer>& read = reads->nextRead();

    // Ignore transition from initial state.
    int prev_state_id = kmerToLexicographicPos(read[0].kmer_);
    for (int i = 1; i < (int)read.size(); i++) {
      int next_state = kmerToLexicographicPos(read[i].kmer_);

      if (read[i].move_ > move_threshold)
        throw std::runtime_error("Found move longer than " +
                                 std::to_string(move_threshold));

      count_for_transition[std::pair<int, int>(prev_state_id, next_state)]++;
      prev_state_id = next_state;
    }
  }

  // Finally, calculate transition probabilities from every state.
  std::vector<std::vector<Transition>> res(states);
  for (int id = 1; id < states; id++) {
    std::string kmer = kmerInLexicographicPos(id, k);
    std::unordered_set<std::string> next_kmers =
        kmersUpToDist(kmer, move_threshold);

    // Number of all transitions going from @id. Sum of all counts.
    int all_transitions = 0;

    // List of transitions going from state @id.
    // Contains (next_state_id, how many times the transition occured).
    std::vector<std::pair<int, int>> transition_with_counts;
    for (const std::string& next_kmer : next_kmers) {
      int next_id = kmerToLexicographicPos(next_kmer);
      int count =
          pseudo_count + count_for_transition[std::pair<int, int>(id, next_id)];
      transition_with_counts.push_back(std::pair<int, int>(next_id, count));
      all_transitions += count;
    }

    // Calculate probability for all transitions going from state @id.
    for (const std::pair<int, int>& transition : transition_with_counts) {
      double prob = transition.second / (double)all_transitions;
      res[id].push_back({transition.first, Log2Num(prob)});
    }
  }

  // Transition probabilities from initial state.
  Log2Num prob(1 / (double)(states - 1));
  for (int id = 1; id < states; id++) {
    res[kInitialState].push_back({id, prob});
  }

  return res;
}
