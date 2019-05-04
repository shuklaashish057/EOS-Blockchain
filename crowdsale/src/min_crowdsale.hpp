#pragma once

#include <string>
#include <vector>

#include <eosiolib/eosio.hpp>
#include <eosiolib/name.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/system.h>
#include <eosiolib/time.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/asset.hpp>

CONTRACT min_crowdsale : public eosio::contract
{
  public:
    // constructor
    min_crowdsale(eosio::name self, eosio::name code, eosio::datastream<const char *> ds);

    // destructor
    ~min_crowdsale();

    ACTION init(eosio::name issuer, eosio::time_point_sec start, eosio::time_point_sec finish); // initialize the crowdsale

    ACTION transfer(eosio::name from, eosio::name to, eosio::asset quantity, std::string memo); // redirect to invest

    ACTION setstart(eosio::time_point_sec start); // start crowdsale

    ACTION setfinish(eosio::time_point_sec finish); // stop a crowdsale

    ACTION withdraw(eosio::name issuer, eosio::time_point_sec finish, eosio::asset quantity); // transfer tokens from the contract account to the issuer

  private:
    // type for defining state
    struct state_t
    {
        eosio::name issuer;
        uint64_t total_eoses;
        uint64_t total_tokens;
        eosio::time_point_sec start;
        eosio::time_point_sec finish;
        eosio::asset quantity;

        // utility method for converting this object to string
        std::string toString()
        {
            std::string str = " ISSUER " + this->issuer.to_string() +
                              " EOSES " + std::to_string(this->total_eoses) +
                              " TOKENS " + std::to_string(this->total_tokens) +
                              " START " + std::to_string(this->start.utc_seconds) +
                              " FINISH " + std::to_string(this->finish.utc_seconds);

            return str;
        }
    };

    // table for holding investors information
    TABLE deposit_t
    {
        eosio::name account;
        uint64_t eoses;
        uint64_t tokens;

        uint64_t primary_key() const { return account.value; }
    };

    // persists the state of the aplication in a singleton. Only one instance will be strored in the RAM for this application
    eosio::singleton<"state"_n, state_t> state_singleton;

    // store investors and balances with contributions in the RAM
    eosio::multi_index<"deposit"_n, deposit_t> deposits;

    // hold present state of the application
    state_t state;

    // handle investments on token transfers
    void handle_investment(eosio::name investor, eosio::asset quantity);

    // private function to call issue action from inside the contract
    void inline_issue(eosio::name to, eosio::asset quantity, std::string memo) const
    {
        // define the type for storing issue information
        struct issue
        {
            eosio::name to;
            eosio::asset quantity;
            std::string memo;
        };

        // create an instance of the action sender and call send function on it
        eosio::action issue_action = eosio::action(
            eosio::permission_level(this->_self, "active"_n),
            eosio::name("eosio.token"), // name of the contract
            eosio::name("issue"),
            issue{to, quantity, memo});

        issue_action.send();
    }

    // private function to handle token transafers
    void inline_transfer(eosio::name from, eosio::name to, eosio::asset quantity, std::string memo) const
    {
        struct transfer
        {
            eosio::name from;
            eosio::name to;
            eosio::asset quantity;
            std::string memo;
        };

        eosio::action transfer_action = eosio::action(
            eosio::permission_level(_self, eosio::name("active")),
            eosio::name("eosio.token"), // name of the contract
            eosio::name("transfer"),
            transfer{from, to, quantity, memo});

        transfer_action.send();
    }

    // a utility function to return default parameters for the state of the crowdsale
    state_t default_parameters() const
    {
        state_t ret;
        ret.total_eoses = 0;
        ret.total_tokens = 0;
        ret.start = eosio::time_point_sec(0);
        ret.finish = eosio::time_point_sec(0);

        return ret;
    }
};