import sys
from eosfactory.eosf import *
verbosity([Verbosity.INFO, Verbosity.OUT, Verbosity.DEBUG])

CONTRACT_WORKSPACE = sys.path[0] + "/../"
TOKEN_CONTRACT_WORKSPACE = sys.path[0] + "/../../quilltoken/"


def test():
    SCENARIO('''
    Execute simple actions.
    ''')
    reset()
    create_master_account("eosio")

    #######################################################################################################
    COMMENT('''
    Create test accounts:
    ''')

    # test accounts and accounts where the smart contracts will be hosted
    create_account("alice", eosio, account_name="alice")
    create_account("crowdsale", eosio, account_name="crowdsale")
    create_account("eosiotoken", eosio, account_name="eosio.token")
    create_account("issuer", eosio, account_name="issuer")

    ########################################################################################################
    COMMENT('''
    Build and deploy token contract:
    ''')

    # creating token contract
    token_contract = Contract(eosiotoken, TOKEN_CONTRACT_WORKSPACE)
    token_contract.build(force=True)
    token_contract.deploy()

    ########################################################################################################
    COMMENT('''
    Build and deploy crowdsale contract:
    ''')

    # creating crowdsale contract
    contract = Contract(crowdsale, CONTRACT_WORKSPACE)
    contract.build(force=True)
    contract.deploy()

    ########################################################################################################
    COMMENT('''
    Create SYS tokens 
    ''')

    token_contract.push_action(
        "create",
        {
            "issuer": eosio,
            "maximum_supply": "1000000000.0000 SYS"
        },
        [eosiotoken]
    )

     ########################################################################################################
    COMMENT('''
    Create QUI tokens 
    ''')

    token_contract.push_action(
        "create",
        {
            "issuer": crowdsale,
            "maximum_supply": "1000000000.0000 QUI"
        },
        [eosiotoken]
    )

    ########################################################################################################
    COMMENT('''
    Issue SYS tokens to alice 
    ''')

    token_contract.push_action(
        "issue",
        {
            "to": alice,
            "quantity": "100000.0000 SYS",
            "memo": "issued tokens to alice"
        },
        [eosio]
    )

    ########################################################################################################
    COMMENT('''
    Create QUILL tokens 
    ''')

    token_contract.push_action(
        "create",
        {
            "issuer": issuer,
            "maximum_supply": "1000000000.0000 QUILL"
        },
        [eosiotoken]
    )

    ########################################################################################################
    COMMENT('''
    Initialize the crowdsale
    ''')

    contract.push_action(
        "init",
        {
            "issuer": issuer,
            "start": "2019-02-01T00:00:00",
            "finish": "2020-04-20T00:00:00"
        },
        [crowdsale]
    )

    ########################################################################################################
    COMMENT('''
    Invest in the crowdsale 
    ''')

    # set eosio.code permission to the contract
    crowdsale.set_account_permission(
        Permission.ACTIVE,
        {
            "threshold": 1,
            "keys": [
                {
                    "key": crowdsale.active(),
                    "weight": 1
                }
            ],
            "accounts":
            [
                {
                    "permission":
                        {
                            "actor": crowdsale,
                            "permission": "eosio.code"
                        },
                    "weight": 1
                }
            ]
        },
        Permission.OWNER,
        (crowdsale, Permission.OWNER)
    )

    # transfer EOS tokens from alice to the host (contract) accounts
    eosiotoken.push_action(
        "transfer",
        {
            "from": alice,
            "to": crowdsale,
            "quantity": "25.0000 SYS",
            "memo": "Invested 25 SYS in crowdsale"
        },
        permission=(alice, Permission.ACTIVE)
    )

    ########################################################################################################
    COMMENT('''
    Check table of the crowdsale contract 
    ''')

    crowdsale.table("deposit", crowdsale)

    stop()


if __name__ == "__main__":
    test()
