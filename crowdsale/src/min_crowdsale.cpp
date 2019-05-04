#include "min_crowdsale.hpp"

#include "config.h"
#include "pow10.hpp"
// #include "src/utils/str_expand.hpp"

// utility macro for converting EOS to our tokens
#define EOS2TKN(EOS) (int64_t)((EOS)*POW10(DECIMALS) * RATE / (1.0 * POW10(4) * RATE_DENOM))

#define NOW now() // rename system func that returns time in seconds

// constructor
min_crowdsale::min_crowdsale(eosio::name self, eosio::name code, eosio::datastream<const char *> ds) : eosio::contract(self, code, ds),
                                                                                                       state_singleton(this->_self, this->_self.value), // code and scope both set to the contract's account
                                                                                                       deposits(this->_self, this->_self.value),
                                                                                                       state(state_singleton.exists() ? state_singleton.get() : default_parameters())
{
}

// destructor
min_crowdsale::~min_crowdsale()
{
    this->state_singleton.set(this->state, this->_self); // persist the state of the crowdsale before destroying instance

    eosio::print("Saving state to the RAM ");
    eosio::print(this->state.toString());
}

// initialize the crowdfund
void min_crowdsale::init(eosio::name issuer, eosio::time_point_sec start, eosio::time_point_sec finish)
{
    eosio_assert(!this->state_singleton.exists(), "Already Initialzed");
    eosio_assert(start < finish, "Start must be less than finish");
    require_auth(this->_self);

    // update state
    this->state.issuer = issuer;
    this->state.start = start;
    this->state.finish = finish;
}

// update contract balances and send tokens to the investor
void min_crowdsale::handle_investment(eosio::name investor, eosio::asset quantity)
{
    // hold the reference to the investor stored in the RAM
    auto it = this->deposits.find(investor.value);

    // calculate from EOS to tokens
    int64_t tokens_to_give = EOS2TKN(quantity.amount);

    // if the depositor account was found, store his updated balances
    int64_t entire_eoses = quantity.amount;
    int64_t entire_tokens = tokens_to_give;
    if (it != this->deposits.end())
    {
        entire_eoses += it->eoses;
        entire_tokens += it->tokens;
    }

    // if the depositor was not found create a new entry in the database, else update his balance
    if (it == this->deposits.end())
    {
        this->deposits.emplace(this->_self, [investor, entire_eoses, entire_tokens](auto &deposit) {
            deposit.account = investor;
            deposit.eoses = entire_eoses;
            deposit.tokens = entire_tokens;
        });
    }
    else
    {
        this->deposits.modify(it, this->_self, [investor, entire_eoses, entire_tokens](auto &deposit) {
            deposit.account = investor;
            deposit.eoses = entire_eoses;
            deposit.tokens = entire_tokens;
        });
    }

    // set the amounts to transfer, then call inline issue action to update balances in the token contract
    eosio::asset amount = eosio::asset(tokens_to_give, eosio::symbol("ASH", 4));
    this->inline_issue(investor, amount, "Crowdsale deposit");
}




// handle transfers to this contract
void min_crowdsale::transfer(eosio::name from, eosio::name to, eosio::asset quantity, std::string memo)
{
    eosio::print("Debug: Invested amount ");
    eosio::print(std::to_string(quantity.amount));

    // funds were sent to this contract only
    //eosio_assert(this->_self == to, "Crowdsale must be the reciever");

    // check timings of the eos crowdsale
    eosio_assert(NOW >= this->state.start.utc_seconds, "Crowdsale hasn't started");
    eosio_assert(NOW <= this->state.finish.utc_seconds, "Crowdsale finished");

    // check the minimum and maximum contribution
    eosio_assert(quantity.amount >= MIN_CONTRIB, "Contribution too low");
    eosio_assert((quantity.amount <= MAX_CONTRIB) /*|| !MAX_CONTRIB*/, "Contribution too high");

    // calculate from EOS to tokens
    int64_t tokens_to_give = EOS2TKN(quantity.amount);

    // update total eos obtained and tokens distributed
    this->state.total_eoses += quantity.amount;
    this->state.total_tokens += tokens_to_give;

    // check if the hard cap was reached
    eosio_assert(this->state.total_tokens <= HARD_CAP_TKN, "Hard cap reached");

    // handle investment
    this->handle_investment(from, quantity);
}




void min_crowdsale::setstart(eosio::time_point_sec start)
{
    require_auth(this->state.issuer);
    eosio_assert(NOW <= this->state.start.utc_seconds, "Crowdsale already started");

    this->state.start = start;
}




void min_crowdsale::setfinish(eosio::time_point_sec finish)
{
    require_auth(this->state.issuer);
    eosio_assert(NOW <= this->state.finish.utc_seconds, "Crowdsale already finished");

    //check if greater than start time
    eosio_assert(finish.utc_seconds > this->state.start.utc_seconds, "Finish can not be be");

    this->state.finish = finish;
}

void min_crowdsale::withdraw(eosio::name issuer, eosio::time_point_sec finish, eosio::asset quantity)
{
    require_auth(this->state.issuer);

    eosio_assert(NOW >= this->state.finish.utc_seconds, "Crowdsale not ended yet" );
    eosio_assert(this->state.total_eoses >= SOFT_CAP_TKN, "Soft cap was not reached");

    eosio::asset amount = eosio::asset();
    this->inline_transfer( this->_self, this->state.issuer,this->state.quantity, "hello" );
}

// custom dispatcher that handles token transfers from quillhash111 token contract
extern "C" void apply(uint64_t receiver, uint64_t code, uint64_t action)
{
    if (code == eosio::name("eosio.token").value && action == eosio::name("transfer").value) // handle actions from eosio.token contract
    {
        eosio::execute_action(eosio::name(receiver), eosio::name(code), &min_crowdsale::transfer);
    }
    else if (code == receiver) // for other (direct) actions
    {
        switch (action)
        {
            EOSIO_DISPATCH_HELPER(min_crowdsale, (init)(transfer)(setstart)(setfinish)(withdraw));
        }
    }
}