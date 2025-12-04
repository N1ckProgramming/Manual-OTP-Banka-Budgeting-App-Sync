# Quick JSON CSV transaction imported script (can only import Retail, Prepaid, and Savings checking account transactions, all of them, just two of them, or just one of them)

# IMPORTANT: BEFORE RUNNING THE CODE, MAKE A BACKUP JSON FILE OF YOUR TRANSACTIONS UNTIL NOW (under settings, go to export and save .json file)

## How to:
When you run the script, you have an input .json, multiple .csv files for each account (if you don't want to import for one of the three checking accounts, then leave ""), and at the end you also have to provide the keys that represent each OTP Bank checking account (retail, prepaid and savings checking account) from the input JSON file and put "entries_" infront of the id.

populate_json.cpp
Compile: g++ -std=c++17 populate_json.cpp -o populate_json
Run:
./populate_json "input.json" "ACCOUNT_RETAIL.csv" "ACCOUNT_PREPAID.csv" "SAVING_HV.csv" "output.json" "json_key_for_retail" "json_key_for_prepaid" "json_key_for_saving"

# Example usage (in the example below, we will only import retail and savings checking accounts and not prepaid account):
g++ main.cpp "input.json" "/mnt/data/SAVING_HV~2094182094810948209~12834019-promet.csv" "" "/mnt/data/SAVING_HV~2913809281039820~31424124-promet.csv" "output.json" "entries_cea64cz1-2223-5be2-aa08-12d14f21z722" "entries_cea64cz1-2223-5be2-aa08-12d14f21z723" "entries_cea64cz1-2223-5be2-aa08-12d14f21z724"
