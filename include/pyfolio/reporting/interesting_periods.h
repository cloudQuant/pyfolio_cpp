#pragma once

#include "../core/datetime.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace pyfolio {
namespace reporting {

/**
 * @brief Represents a significant time period for analysis
 */
struct InterestingPeriod {
    std::string name;
    DateTime start;
    DateTime end;
    std::string description;

    InterestingPeriod(const std::string& name, const DateTime& start, const DateTime& end,
                      const std::string& description = "")
        : name(name), start(start), end(end), description(description) {}
};

/**
 * @brief Collection of predefined interesting time periods
 *
 * These periods represent significant market events that are useful
 * for stress testing and performance analysis
 */
class InterestingPeriods {
  public:
    /**
     * @brief Get all predefined interesting periods
     */
    static std::vector<InterestingPeriod> get_all_periods() {
        return {
            // Market crises
            {                "Dot-com Crash", DateTime(2000,  3, 10), DateTime(2002, 10,  9),"Technology bubble burst"                                                                                             },

            {                 "9/11 Attacks", DateTime(2001,  9, 11), DateTime(2001,  9, 21),
             "Market closure and aftermath of terrorist attacks"                                                                    },

            {      "Global Financial Crisis", DateTime(2007, 10,  9), DateTime(2009,  3,  9),
             "Subprime mortgage crisis and Great Recession"                                                                         },

            {                  "Flash Crash", DateTime(2010,  5,  6), DateTime(2010,  5,  7),   "Intraday market crash and recovery"},

            {         "European Debt Crisis", DateTime(2011,  8,  1), DateTime(2011, 10, 31),     "Eurozone sovereign debt concerns"},

            {"China Stock Market Turbulence", DateTime(2015,  6, 12), DateTime(2016,  2, 11),
             "Chinese market volatility and global spillover"                                                                       },

            {                  "Brexit Vote", DateTime(2016,  6, 23), DateTime(2016,  6, 27),     "UK EU referendum market reaction"},

            {               "COVID-19 Crash", DateTime(2020,  2, 19), DateTime(2020,  3, 23),        "Pandemic-induced market crash"},

            {            "COVID-19 Recovery", DateTime(2020,  3, 23), DateTime(2020,  8, 31),           "Post-crash recovery period"},

            // Volatility events
            {           "Vol Spike Feb 2018", DateTime(2018,  2,  2), DateTime(2018,  2,  9), "XIV termination and volatility spike"},

            {              "Q4 2018 Selloff", DateTime(2018, 10,  1), DateTime(2018, 12, 24),  "Rate hike fears and growth concerns"},

            // Rate cycles
            {            "Fed Taper Tantrum", DateTime(2013,  5, 22), DateTime(2013,  9, 30),
             "Federal Reserve tapering announcement"                                                                                },

            {    "Rate Hike Cycle 2015-2018", DateTime(2015, 12, 16), DateTime(2018, 12, 19),
             "Federal Reserve rate normalization"                                                                                   },

            // Geopolitical events
            {  "Russian Invasion of Ukraine", DateTime(2022,  2, 24), DateTime(2022,  3, 31),
             "Initial market reaction to invasion"                                                                                  },

            // Sector-specific events
            { "Oil Price Collapse 2014-2016", DateTime(2014,  6, 20), DateTime(2016,  2, 11),                "Crude oil bear market"},

            {         "Tech Rally 2020-2021", DateTime(2020,  3, 23), DateTime(2021, 12, 31),     "Technology sector outperformance"},

            // Inflation periods
            {    "Inflation Surge 2021-2022", DateTime(2021,  3,  1), DateTime(2022, 12, 31),
             "Post-pandemic inflation spike"                                                                                        },

            // Banking crises
            {    "Regional Bank Crisis 2023", DateTime(2023,  3,  8), DateTime(2023,  3, 27),
             "SVB collapse and regional bank stress"                                                                                }
        };
    }

    /**
     * @brief Get periods by category
     */
    static std::unordered_map<std::string, std::vector<InterestingPeriod>> get_periods_by_category() {
        std::unordered_map<std::string, std::vector<InterestingPeriod>> categorized;

        categorized["Crises"] = {
            {            "Dot-com Crash", DateTime(2000,  3, 10), DateTime(2002, 10,  9)},
            {  "Global Financial Crisis", DateTime(2007, 10,  9), DateTime(2009,  3,  9)},
            {           "COVID-19 Crash", DateTime(2020,  2, 19), DateTime(2020,  3, 23)},
            {"Regional Bank Crisis 2023", DateTime(2023,  3,  8), DateTime(2023,  3, 27)}
        };

        categorized["Volatility Events"] = {
            {       "Flash Crash", DateTime(2010,  5, 6), DateTime(2010,  5,  6)},
            {"Vol Spike Feb 2018", DateTime(2018,  2, 2), DateTime(2018,  2,  9)},
            {   "Q4 2018 Selloff", DateTime(2018, 10, 1), DateTime(2018, 12, 24)}
        };

        categorized["Central Bank"] = {
            {        "Fed Taper Tantrum", DateTime(2013,  5, 22), DateTime(2013,  9, 30)},
            {"Rate Hike Cycle 2015-2018", DateTime(2015, 12, 16), DateTime(2018, 12, 19)}
        };

        categorized["Geopolitical"] = {
            {               "9/11 Attacks", DateTime(2001, 9, 11), DateTime(2001, 9, 21)},
            {                "Brexit Vote", DateTime(2016, 6, 23), DateTime(2016, 6, 27)},
            {"Russian Invasion of Ukraine", DateTime(2022, 2, 24), DateTime(2022, 3, 31)}
        };

        return categorized;
    }

    /**
     * @brief Get recent interesting periods (last N years)
     */
    static std::vector<InterestingPeriod> get_recent_periods(int years = 5) {
        auto all_periods = get_all_periods();
        auto cutoff_date = DateTime::now().add_days(-years * 365);

        std::vector<InterestingPeriod> recent;
        for (const auto& period : all_periods) {
            if (period.start >= cutoff_date) {
                recent.push_back(period);
            }
        }

        return recent;
    }

    /**
     * @brief Get periods that overlap with a given date range
     */
    static std::vector<InterestingPeriod> get_overlapping_periods(const DateTime& start_date,
                                                                  const DateTime& end_date) {
        auto all_periods = get_all_periods();
        std::vector<InterestingPeriod> overlapping;

        for (const auto& period : all_periods) {
            // Check if periods overlap
            if (period.start <= end_date && period.end >= start_date) {
                overlapping.push_back(period);
            }
        }

        return overlapping;
    }

    /**
     * @brief Add a custom interesting period
     */
    static void add_custom_period(const InterestingPeriod& period) { custom_periods_.push_back(period); }

    /**
     * @brief Get all periods including custom ones
     */
    static std::vector<InterestingPeriod> get_all_including_custom() {
        auto periods = get_all_periods();
        periods.insert(periods.end(), custom_periods_.begin(), custom_periods_.end());
        return periods;
    }

    /**
     * @brief Clear custom periods
     */
    static void clear_custom_periods() { custom_periods_.clear(); }

  private:
    static std::vector<InterestingPeriod> custom_periods_;
};

// Define static member
inline std::vector<InterestingPeriod> InterestingPeriods::custom_periods_;

}  // namespace reporting
}  // namespace pyfolio