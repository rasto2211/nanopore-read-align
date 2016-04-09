#include <vector>
#include <string>
#include <iostream>

#include "src/hmm.h"
#include "src/move_hmm.h"
#include "src/fast5_reads.h"
#include "src/kmers.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

using ::testing::Return;
using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAreArray;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::DoubleEq;

TEST(MoveHMMTest, ConstructEmissionsTest) {
  std::vector<GaussianParamsKmer> gaussians = {
      {"G", 1, 0.1}, {"A", 0, 0.5}, {"T", 0.5, 0.2}, {"C", 0.5, 0.1}};

  std::vector<State<double>*> emissions = constructEmissions(1, gaussians);

  ASSERT_EQ(5, emissions.size());

  ASSERT_NE(nullptr, emissions[0]);
  EXPECT_EQ(SilentState<double>(), *emissions[0]);

  ASSERT_NE(nullptr, emissions[1]);
  EXPECT_EQ(GaussianState(0, 0.5), *emissions[1]);

  ASSERT_NE(nullptr, emissions[2]);
  EXPECT_EQ(GaussianState(0.5, 0.1), *emissions[2]);

  ASSERT_NE(nullptr, emissions[3]);
  EXPECT_EQ(GaussianState(0.5, 0.2), *emissions[3]);

  ASSERT_NE(nullptr, emissions[4]);
  EXPECT_EQ(GaussianState(1, 0.1), *emissions[4]);
}

class MockFast5Reads : public Fast5Reads {
 public:
  MockFast5Reads(const std::string& path) : Fast5Reads(path) {}
  MOCK_METHOD0(nextRead, std::vector<MoveKmer>());
  MOCK_CONST_METHOD0(hasNextRead, bool());
};

const int kMoveThreshold = 3;

// total_transitions_from_state are without pseudocount.
// Pseudocount is added for every transition going from the state so we don't
// have zero probabilities.
void test_for_transition(const std::vector<std::vector<Transition>>& res,
                         const std::string& from, const std::string& to,
                         int count_with_pseudocount,
                         int total_transitions_from_state) {
  EXPECT_THAT(res[kmerToLexicographicPos(from)],
              Contains<Transition>(
                  {kmerToLexicographicPos(to),
                   Log2Num(count_with_pseudocount /
                           (double)(kmersUpToDist(from, kMoveThreshold).size() +
                                    total_transitions_from_state))}))
      << "From: " << from << "(id = " << kmerToLexicographicPos(from) << ")";
}

// Larger test with k=4.
TEST(MoveHMMTest, ConstructTransitionsLargeTest) {
  ::testing::StrictMock<MockFast5Reads> reads_mock("path");

  // We have only two reads.
  EXPECT_CALL(reads_mock, hasNextRead())
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(false));

  std::vector<MoveKmer> read1 = {{0, "ACTC"},
                                 {0, "ACTC"},
                                 {1, "CTCA"},
                                 {2, "CAGC"},
                                 {0, "CAGC"},
                                 {3, "CTCA"}};
  std::vector<MoveKmer> read2 = {
      {0, "CTCA"}, {1, "CAGC"}, {3, "CTCA"}, {0, "CTCA"}};
  EXPECT_CALL(reads_mock, nextRead()).WillOnce(Return(read1)).WillOnce(
      Return(read2));

  const int k = 4;
  const int kPseudoCount = 1;
  std::vector<std::vector<Transition>> res =
      constructTransitions(kMoveThreshold, kPseudoCount, k, &reads_mock);

  const int kmers = 256;  // Number of kmers of length 4.
  ASSERT_EQ(kmers + 1, res.size());

  // Initial state.
  ASSERT_EQ(kmers, res[0].size());
  int id = 1;
  for (const Transition& transition : res[0]) {
    // Probability of every kmer is equal.
    EXPECT_DOUBLE_EQ(1 / (double)kmers, transition.prob_.value());
    EXPECT_EQ(id++, transition.to_state_);
  }

  // Check transitions for other states. Do not check transitions with
  // zeros count. It would be too much.
  test_for_transition(res, "ACTC", "ACTC", 2, 2);
  test_for_transition(res, "ACTC", "CTCA", 2, 2);
  test_for_transition(res, "CTCA", "CAGC", 3, 3);
  test_for_transition(res, "CTCA", "CTCA", 2, 3);
  test_for_transition(res, "CAGC", "CTCA", 3, 3);
  test_for_transition(res, "CAGC", "CAGC", 2, 3);

  // Number of transitions for every state > 0.
  for (int state = 1; state <= kmers; state++) {
    EXPECT_EQ(
        kmersUpToDist(kmerInLexicographicPos(state, k), kMoveThreshold).size(),
        res[state].size());
  }
}

TEST(MoveHMMTest, ConstructTransitionsSmallTest) {
  ::testing::StrictMock<MockFast5Reads> reads_mock("path");

  EXPECT_CALL(reads_mock, hasNextRead()).WillOnce(Return(true)).WillOnce(
      Return(false));

  std::vector<MoveKmer> read1 = {
      {0, "AG"}, {1, "GA"}, {1, "AG"}, {1, "GA"}, {1, "AG"}, {2, "TG"}};
  EXPECT_CALL(reads_mock, nextRead()).WillOnce(Return(read1));

  const int kPseudoCount = 1;
  const int k = 2;
  std::vector<std::vector<Transition>> res =
      constructTransitions(kMoveThreshold, kPseudoCount, k, &reads_mock);

  const int kmers = 16;  // Number of kmers of length 2.
  ASSERT_EQ(kmers + 1, res.size());

  // Initial state.
  ASSERT_EQ(kmers, res[0].size());
  int id = 1;
  for (const Transition& transition : res[0]) {
    // Probability of every kmer is equal.
    EXPECT_DOUBLE_EQ(1 / (double)kmers, transition.prob_.value());
    EXPECT_EQ(id++, transition.to_state_);
  }

  test_for_transition(res, "AG", "GA", 3, 3);
  test_for_transition(res, "AG", "TG", 2, 3);
  test_for_transition(res, "GA", "AG", 3, 2);

  // Transitions with zero count. Only pseudocount is added to achieve non-zero
  // probability. Not all zero-count transitions are listed. There are 16^2
  // transitions.
  test_for_transition(res, "AG", "AG", 1, 3);
  test_for_transition(res, "AG", "AA", 1, 3);
  test_for_transition(res, "AG", "AC", 1, 3);
  test_for_transition(res, "AG", "CC", 1, 3);
  test_for_transition(res, "AG", "TT", 1, 3);

  test_for_transition(res, "AA", "AA", 1, 0);
  test_for_transition(res, "CC", "CC", 1, 0);
  test_for_transition(res, "TT", "TT", 1, 0);

  // Number of transitions for every state > 0.
  for (int state = 1; state <= kmers; state++) {
    EXPECT_EQ(kmers, res[state].size());
  }
}

TEST(MoveHMMTest, ConstructTransitionsExceptionTest) {
  ::testing::StrictMock<MockFast5Reads> reads_mock("path");

  EXPECT_CALL(reads_mock, hasNextRead()).WillOnce(Return(true));

  std::vector<MoveKmer> read1 = {{0, "ACG"}, {2, "GTG"}};
  EXPECT_CALL(reads_mock, nextRead()).WillOnce(Return(read1));

  const int kPseudoCount = 1;
  const int k = 3;
  const int kMoveThresholdException = 1;
  EXPECT_THROW(constructTransitions(kMoveThresholdException, kPseudoCount, k,
                                    &reads_mock),
               std::runtime_error);
}
