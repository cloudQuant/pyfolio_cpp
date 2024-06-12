#include "gtest/gtest.h"
#include "../include/pyfolio/pos.h"

class PosTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test data
        DateTime dt1{"2021-01-01"}, dt2{"2021-01-02"};
        df.index = {dt1, dt2};
        df.cols = {"A", "B", "C"};
        df.string_index = {"2021-01-01", "2021-01-02"};
        df.values = {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}};
    }

    MyDataFrame df;
};

TEST_F(PosTest, test_get_percent_allocation_1) {
    MyDataFrame result = get_percent_allocation(df);
    std::vector<std::vector<double>> expected_values = {{1.0/6, 2.0/6, 3.0/6}, {4.0/15, 5.0/15, 6.0/15}};

    EXPECT_EQ(result.values.size(), expected_values.size());
    for (size_t i = 0; i < expected_values.size(); ++i) {
        for (size_t j = 0; j < expected_values[i].size(); ++j) {
            EXPECT_NEAR(result.values[i][j], expected_values[i][j], 1e-9);
        }
    }
}

TEST_F(PosTest, test_get_percent_allocation_2) {
    MyDataFrame empty_df;
    MyDataFrame result = get_percent_allocation(empty_df);
    EXPECT_TRUE(result.values.empty());
}

TEST_F(PosTest, test_get_percent_allocation_3) {
    df.values = {{1.0, NAN, 3.0}, {4.0, 5.0, NAN}};
    MyDataFrame result = get_percent_allocation(df);
    std::vector<std::vector<double>> expected_values = {{1.0/4, NAN, 3.0/4}, {4.0/9, 5.0/9, NAN}};

    EXPECT_EQ(result.values.size(), expected_values.size());
    for (size_t i = 0; i < expected_values.size(); ++i) {
        for (size_t j = 0; j < expected_values[i].size(); ++j) {
            if (std::isnan(result.values[i][j])){
                EXPECT_TRUE(std::isnan(expected_values[i][j]));
            }else{
                EXPECT_NEAR(result.values[i][j], expected_values[i][j], 1e-9);
            }

        }
    }
}