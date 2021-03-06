#pragma once

#include <vector>
#include <cmath>
#include <memory>
#include <random>
#include <typeinfo>

#include <json/value.h>

#include "log2_num.h"
#include "gtest/gtest_prod.h"

// Transition from one state to another.
struct Transition {
  int to_state_;
  Log2Num prob_;
  friend inline std::ostream& operator<<(std::ostream& os,
                                         const Transition& rhs);
  inline bool operator==(const Transition& rhs) const {
    return (prob_ == rhs.prob_) && (to_state_ == rhs.to_state_);
  }
};

inline std::ostream& operator<<(std::ostream& os, const Transition& rhs) {
  os << "(" << rhs.to_state_ << ", " << rhs.prob_ << ")";

  return os;
}

template <typename EmissionType>
class State {
 public:
  // Is this state silent?
  virtual bool isSilent() const = 0;
  // Emission probability for the state
  virtual Log2Num prob(const EmissionType& emission) const = 0;

  // These two method are used for serialization.
  std::string toJsonStr() const { return toJsonValue().toStyledString(); }
  virtual Json::Value toJsonValue() const = 0;

  virtual bool operator==(const State<EmissionType>& state) const = 0;
};

// State with no emission. Prob method always returns 1. It's convenient.
template <typename EmissionType>
class SilentState : public State<EmissionType> {
 public:
  SilentState(const Json::Value& /* params */) {}
  SilentState() {};
  bool isSilent() const { return true; }
  Log2Num prob(const EmissionType& /* emission */) const { return Log2Num(1); }
  Json::Value toJsonValue() const;
  bool operator==(const State<EmissionType>& state) const {
    return typeid(*this) == typeid(state);
  }
};

// State with Gaussian emission.
class GaussianState : public State<double> {
 public:
  GaussianState(const Json::Value& params) {
    mu_ = params["mu"].asDouble();
    sigma_ = params["sigma"].asDouble();
  }
  GaussianState(double mu, double sigma) : mu_(mu), sigma_(sigma) {}
  bool isSilent() const { return false; }
  Log2Num prob(const double& emission) const;
  Json::Value toJsonValue() const;

  bool operator==(const State<double>& state) const {
    if (typeid(*this) != typeid(state)) return false;

    const GaussianState& gaussian = dynamic_cast<const GaussianState&>(state);
    return (mu_ == gaussian.mu_) && (sigma_ == gaussian.sigma_);
  }

 protected:
  FRIEND_TEST(GaussianStateTest, GaussianStateDeserializeParamsTest);
  double mu_;
  double sigma_;
};

// Hidden Markov Model with silent states. It has one initial state.
// States for the HMM has to be calculated for every emissions sequence because
// that's the way it is in ONT data.
template <typename EmissionType>
class HMM {
 public:
  HMM() {}

  // States are evaluated in ascending order by id during DP.
  // Therefore we have to put restriction on transitions going
  // to silent state. Let's say that we have transition x->y
  // and y is silent states. Then x<y has to hold true.
  // No transition can go to initial state and initial state is silent.
  HMM(int initial_state,
      const std::vector<std::vector<Transition>>& transitions);

  // Constructs HMM from JSON.
  HMM(const Json::Value& hmm_json);

  // Runs Viterbi algorithm and returns sequence of states.
  // @emission_seq - sequence of emissions - MinION read.
  // @states - states of HMM.
  std::vector<int> runViterbiReturnStateIds(
      const std::vector<EmissionType>& emission_seq,
      const std::vector<std::unique_ptr<State<EmissionType>>>& states) const;
  // Samples from P(state_sequence|emission_sequence) and returns n sequences of
  // states.
  // @samples - number of samples in the result.
  // @seed - seed used for random number generator.
  // @states - states of HMM.
  std::vector<std::vector<int>> posteriorProbSample(
      int samples, int seed, const std::vector<EmissionType>& emissions,
      const std::vector<std::unique_ptr<State<EmissionType>>>& states) const;

  // Serializes transitions to JSON.
  std::string toJsonStr() const;

 private:
  FRIEND_TEST(HMMTest, ComputeViterbiMatrixTest);
  FRIEND_TEST(HMMTest, ForwardTrackingTest);
  FRIEND_TEST(HMMTest, ComputeInvTransitions);
  FRIEND_TEST(HMMTest, HMMDeserializationTest);

  typedef typename std::pair<Log2Num, int> ProbStateId;
  typedef typename std::vector<std::vector<ProbStateId>> ViterbiMatrix;
  typedef typename std::vector<std::vector<std::vector<double>>> ForwardMatrix;
  typedef typename std::vector<std::vector<std::discrete_distribution<int>>>
      SamplingMatrix;

  // Finds best path to @state after @steps using @prob[steps][state].
  // Helper method for Viterbi algorithm.
  ProbStateId bestPathTo(int state_id, const State<EmissionType>& state,
                         int emissions_prefix_len,
                         const EmissionType& last_emission,
                         const ViterbiMatrix& prob) const;
  // Computes matrix which is used in Viterbi alorithm.
  ViterbiMatrix computeViterbiMatrix(
      const std::vector<EmissionType>& emissions,
      const std::vector<std::unique_ptr<State<EmissionType>>>& states) const;
  // Computes matrix res[i][j][k] which means:
  // Sum of probabilities of all paths of form
  // initial_state -> ... -> inv_transitions_[j][k] -> j
  // and emitting emissions[0... i-1].
  ForwardMatrix forwardTracking(
      const std::vector<EmissionType>& emissions,
      const std::vector<std::unique_ptr<State<EmissionType>>>& states) const;
  std::vector<int> backtrackMatrix(
      int last_state, int last_row,
      const std::vector<std::unique_ptr<State<EmissionType>>>& states,
      const std::function<int(int, int)>& nextState) const;

  // Computes inverse transition.
  void computeInvTransitions();
  // These checks are done:
  // 1) Initial state has to be silent.
  // 2) No transitions can go to initial state.
  // 3) Transition to silent state. Outgoing state has to have lower number.
  void isValid(const std::vector<std::unique_ptr<State<EmissionType>>>& states)
      const;

  // This constant is used in Viterbi algorithm to denote that we cannot get
  // into this state. No previous state.
  const int kNoState = -1;

  int initial_state_;
  // Number of states including the initial state.
  int num_states_;

  // List of transitions from one state to another with probabilities.
  // Ids of states are from 0 to transitions_.size()-1
  std::vector<std::vector<Transition>> transitions_;
  // Inverse transitions.
  std::vector<std::vector<Transition>> inv_transitions_;
};

// Implementation of template classes.
#include "hmm.tcc"
