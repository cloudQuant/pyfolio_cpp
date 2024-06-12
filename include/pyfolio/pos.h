#include <iostream>
#include "../empyrical_cpp/include/empyrical/datetime.h"
#include "../empyrical_cpp/include/empyrical/utils.h"


/*
 * Determines a portfolio's allocations.
 */
inline MyDataFrame get_percent_allocation(const MyDataFrame & df){
    MyDataFrame new_df;

    std::vector<std::vector<double>> values = df.values;
    std::size_t num_rows = values.size();
    std::size_t num_cols = num_rows > 0 ? values[0].size() : 0;
    std::vector<std::vector<double>> new_values(num_rows, std::vector<double>(num_cols));

    for (std::size_t i=0; i<num_rows; i++){
        auto vec = values[i];
        double sum = cal_func::nan_sum(vec);
        for (std::size_t j=0; j< num_cols; j++){
            new_values[i][j] = vec[j]/sum;
        }
    }
    new_df.index = df.index;
    new_df.cols = df.cols;
    new_df.string_index = df.string_index;
    new_df.values = new_values;
    return new_df;
}

