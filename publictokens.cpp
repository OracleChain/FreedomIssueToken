/**
 * author chenlian@oraclechain.io
 * version 1.0
 * source reference eosdactoken contract,thanks eos contracts/eosdactoken contract
 * this eosdactoken follow the erc2.0 norms(trans,issue,approve,transferfrom,balanceof,allowance)
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */
#include "publictokens.hpp"
#include"tool.hpp"
#include <math.h>
#include <boost/algorithm/string.hpp>

using std::string;
using std::array;
using namespace eosio;

#define MEMO_SPLITTER "|"

void eosdactoken::checkasset(const asset &quantity){
    eosio_assert( quantity.symbol.is_valid(), INVALID_SYMBOL_NAME);
    auto sym = quantity.symbol.name();
    Stat statstable( _self, sym );
    const auto& ite = statstable.find( sym );
    eosio_assert( ite != statstable.end(),  TOKEN_WITH_SYMBOL_DOES_NOT_EXIST_CREATE_TOKEN_BEFORE_ISSUE);
    const auto& st = *ite;
    eosio_assert( quantity.is_valid(), INVALID_QUANTITY );
    eosio_assert( quantity.amount > 0, MUST_ISSUE_POSITIVE_QUANTITY );
    eosio_assert( quantity.symbol == st.supply.symbol, SYMBOL_PRECISION_MISMATCH);
    checkoutAmount(quantity.amount);
}

void eosdactoken::issue(account_name to,
                        asset        quantity,
                        string       memo) {

    checkasset(quantity);

    eosio_assert( memo.size() <= 256, MEMO_HAS_MORE_THAN_256_BYTES);
    auto sym = quantity.symbol.name();
    Stat statstable( _self, sym );
    const auto& ite = statstable.find( sym );
    const auto& st = *ite;
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, QUANTITY_EXCEEDS_AVAILABLE_SUPPLY);

    require_auth( st.issuer);

    statstable.modify( st, 0, [&]( auto& s ) {
        s.supply += quantity;
    });

    add_balance( st.issuer, quantity,  st.issuer);

    if( to != st.issuer )
    {
        SEND_INLINE_ACTION( *this, transfer, {st.issuer,N(active)}, {st.issuer, to, quantity, memo} );
    }
}

void eosdactoken::transferfee(
        account_name from,
        account_name to,
        asset        quantity,
        account_name tofeeadmin,
        asset        feequantity,
        string       memo){

        checkasset(quantity);
        checkasset(feequantity);
        eosio_assert( quantity.symbol == feequantity.symbol, TOKEN_SYMBOL_SHOULD_EQUAL_FEE_SYMBOL);

        require_auth(from);

        eosio_assert( from != to, CANNOT_TRANSFER_TO_YOURSELF);
        eosio_assert( from != tofeeadmin, CANNOT_TRANSFER_TO_YOURSELF);

        eosio_assert( is_account( to ), TO_ACCOUNT_DOES_NOT_EXIST);
        eosio_assert( is_account( tofeeadmin ), TO_ACCOUNT_DOES_NOT_EXIST);

        auto sym = quantity.symbol.name();
        Stat statstable( _self, sym );
        const auto& st = statstable.get( sym );

        require_recipient( from );
        require_recipient( to );
        require_recipient( tofeeadmin );

        sub_balance( from, quantity, from );
        add_balance( to, quantity, from);

        sub_balance( from, feequantity, from);
        add_balance( tofeeadmin, feequantity, from);
}

void eosdactoken::sub_balance( account_name owner, asset value, uint64_t payer) {

    require_auth(payer);

    Accounts from_acnts( _self, owner );

    const auto& from = from_acnts.get( value.symbol.name(),  NO_BALANCE_OBJECT_FOUND_FOR_THIS_ACCOUNT);
    eosio_assert( from.balance.amount >= value.amount, BLANCE_NOT_ENOUGH );

    if( from.balance.amount == value.amount ) {
        from_acnts.erase( from );
        } else {
        from_acnts.modify( from, payer, [&]( auto& a ) {
            a.balance -= value;
        });
    }
}

void eosdactoken::add_balance( account_name owner, asset value, account_name payer )
{
    require_auth(payer);

    Accounts to_acnts( _self, owner );
    auto to = to_acnts.find( value.symbol.name() );
    if( to == to_acnts.end() ) {
        to_acnts.emplace( payer, [&]( auto& a ){
            a.balance = value;
        });
    } else {
        to_acnts.modify( to, payer, [&]( auto& a ) {
        a.balance += value;
        });
    }
}


void eosdactoken::approve(account_name owner,
                        account_name spender,
                        asset quantity){

    eosio_assert( quantity.symbol.is_valid(), INVALID_SYMBOL_NAME);
    auto sym = quantity.symbol.name();
    Stat statstable( _self, sym );
    const auto& ite = statstable.find( sym );
    eosio_assert( ite != statstable.end(),  TOKEN_WITH_SYMBOL_DOES_NOT_EXIST_CREATE_TOKEN_BEFORE_ISSUE);
    const auto& st = *ite;
    eosio_assert( quantity.is_valid(), INVALID_QUANTITY );
    eosio_assert( quantity.amount >= 0, MUST_ISSUE_POSITIVE_QUANTITY );
    eosio_assert( quantity.symbol == st.supply.symbol, SYMBOL_PRECISION_MISMATCH);
    checkoutAmount(quantity.amount);

    require_auth(owner);

    eosio_assert( is_account( spender ), TO_ACCOUNT_DOES_NOT_EXIST);

    Accounts from_acnts( _self, owner );
    const auto& from = from_acnts.find( quantity.symbol.name());

    eosio_assert(from!=from_acnts.end(),  YOU_NOT_HAVE_THIS_TOKEN_NOW);

    eosio_assert( from->balance.amount >= quantity.amount, BLANCE_NOT_ENOUGH );

    Approves approveobj(_self, owner);
    if(approveobj.find(quantity.symbol.name()) != approveobj.end()){

        auto &approSymIte = approveobj.get(quantity.symbol.name());

        approveobj.modify(approSymIte, owner, [&](auto &a){

            auto approvetoPairIte = a.approved.begin();

            bool finded = false;
            while(approvetoPairIte != a.approved.end()){

                if(approvetoPairIte->to == spender){

                    finded = true;
                    if(quantity.amount == 0){
                        a.approved.erase(approvetoPairIte);
                    }else{
                        approvetoPairIte->value = quantity.amount;
                    }
                    break;
                }
                approvetoPairIte++;
            }

            if(!finded && quantity.amount>0){
                approvetoPair atp;
                atp.to = spender;
                atp.value = quantity.amount;
                a.approved.push_back(atp);
            }
        });
    }else if(quantity.amount>0){

        approvetoPair atp;
        atp.to = spender;
        atp.value = quantity.amount;

        approveobj.emplace(owner, [&](auto &a){
            a.symbol_name = quantity.symbol;
            a.approved.push_back(atp);
        });
    }
}

void eosdactoken::balanceof(account_name owner,
                            std::string  symbol){
    symbol_name sn = string_to_symbol(4, symbol.c_str());
    print("balanceOf[",symbol.c_str(),"]=", get_balance(owner, symbol_type(sn).name()));
}

void eosdactoken::allowance(account_name owner,
                            account_name spender,
                            std::string  symbol){
    Approves approveobj(_self, owner);

    symbol_type st = symbol_type(string_to_symbol(4, symbol.c_str()));
        if(approveobj.find(st.name()) != approveobj.end()){
            const auto &approSymIte = approveobj.get(st.name());
            auto approvetoPairIte = approSymIte.approved.begin();
            while(approvetoPairIte != approSymIte.approved.end()){

                if(approvetoPairIte->to == spender){
                    print("allowanceof[", account_name(approvetoPairIte->to), "]=", approvetoPairIte->value);
                    return;
                }
                approvetoPairIte++;
            }
            if(approvetoPairIte == approSymIte.approved.end()){
                print("allowanceOf[", account_name(spender), "]=", 0);
            }

        }else{
            print("allowanceOf[", (account_name)spender, "]=", 0);
        }
}

void eosdactoken::transferfrom(account_name owner,
                            account_name spender,
                            asset quantity){

    checkasset(quantity);

    require_auth(spender);

    eosio_assert( is_account( owner ), TO_ACCOUNT_DOES_NOT_EXIST);


    eosio_assert( quantity.amount > 0, MUST_ISSUE_POSITIVE_QUANTITY);

    Approves approveobj(_self, owner);
    if(approveobj.find(quantity.symbol.name()) != approveobj.end()){

        const auto &approSymIte = approveobj.get(quantity.symbol.name());
        approveobj.modify(approSymIte, owner, [&](auto &a){
            a = approSymIte;
            auto approvetoPairIte = a.approved.begin();
            bool finded = false;
            while(approvetoPairIte != a.approved.end()){

                if(approvetoPairIte->to == spender){
                    finded = true;
                    eosio_assert(approvetoPairIte->value>=quantity.amount, NOT_ENOUGH_ALLOWED_OCT_TO_DO_IT);
                    eosio_assert(approvetoPairIte->value > approvetoPairIte->value-quantity.amount, NOT_ENOUGH_ALLOWED_OCT_TO_DO_IT);
                    approvetoPairIte->value -= quantity.amount;

                    checkoutAmount(approvetoPairIte->value);

                    if(approvetoPairIte->value == 0){
                        a.approved.erase(approvetoPairIte);
                    }

                    require_recipient( spender );
                    sub_balance( owner, quantity, spender);
                    add_balance( spender,   quantity, spender);
                    break;
                }
                approvetoPairIte++;
            }
            if(!finded){
                eosio_assert(false, NOT_ENOUGH_ALLOWED_OCT_TO_DO_IT);
            }
        });
    }else{
        eosio_assert(false, NOT_ENOUGH_ALLOWED_OCT_TO_DO_IT);
    }
}

void eosdactoken::create(       account_name           issuer,
                                asset                  currency) {
    require_auth( _self );

    auto sym = currency.symbol;
    eosio_assert( sym.is_valid(), INVALID_SYMBOL_NAME);
    eosio_assert( currency.is_valid(), INVALID_QUANTITY);
    eosio_assert( currency.amount>0, TOKEN_MAX_SUPPLY_MUST_POSITIVE) ;

    Stat statstable( _self, sym.name() );
    auto existing = statstable.find( sym.name() );
    eosio_assert( existing == statstable.end(), TOKEN_WITH_SYMBOL_ALREADY_EXISTS);

    statstable.emplace( _self, [&]( auto& s ) {
        s.supply.symbol = currency.symbol;
        s.max_supply    = currency;
        s.issuer        = issuer;
    });
}

void eosdactoken::copystates(std::string symbol){
    require_auth( _self );

    auto sym = symbol_type(string_to_symbol(4, symbol.c_str())).name();
    Stat statstable( _self, sym );
    auto existing = statstable.find( sym );
    const auto& st = *existing;

    eosio_assert( existing != statstable.end(), TOKEN_WITH_SYMBOL_NOT_EXISTS);

    Stat stattable( _self, sym );
    const auto& stdest = stattable.find(sym);
    if(stdest!=stattable.end()){
        stattable.modify( stdest, 0, [&]( auto& s ) {
            s.supply        = st.supply;
            s.max_supply    = st.max_supply;
            s.issuer        = st.issuer;
            });
    }else{
        stattable.emplace( _self, [&]( auto& s ) {
            s.supply        = st.supply;
            s.max_supply    = st.max_supply;
            s.issuer        = st.issuer;
        });
    }
}

void eosdactoken::transfer(  account_name from, account_name to, asset quantity, string memo) {
    require_auth(from);

    eosio_assert( from != to, CANNOT_TRANSFER_TO_YOURSELF);
    eosio_assert( is_account( to ), TO_ACCOUNT_DOES_NOT_EXIST);

    require_recipient( from );
    require_recipient( to );

    eosio_assert( memo.size() <= 256, MEMO_HAS_MORE_THAN_256_BYTES);

    if (the_code == N(eosio.token)) {
        issue_token(from, to, quantity, memo);
    } else {
        transfer_token(from, to, quantity, memo);
    }
}

void eosdactoken::transfer_token(account_name from, account_name to, asset quantity, string memo)
{
    checkasset(quantity);

    auto sym = quantity.symbol.name();
    Stat statstable( _self, sym );
    const auto& st = statstable.get( sym );

    sub_balance( from, quantity, from);
    add_balance( to, quantity, from);
}

void eosdactoken::issue_token(account_name from, account_name to, asset quantity, string memo)
{
    // issue token
    // memo should be: ISSUE TOKEN:100000000|5|ABC
    if (to == _self && quantity.symbol == S(4,EOS) && memo.find("ISSUE TOKEN:") != string::npos) {
        eosio_assert(quantity.is_valid(), "Invalid token transfer");
        eosio_assert(quantity.amount >= 20000, "Not enough EOS");  // 2.0000 EOS

        string subs = memo.substr(memo.find("ISSUE TOKEN:") + 12, string::npos);
        vector<string> strs;
        boost::split(strs, subs, boost::is_any_of(MEMO_SPLITTER));

        eosio_assert(strs.size() >= 3, "wrong memo format.");

        int64_t a = strtoll(strs.at(0).c_str(), nullptr, 10);
        eosio_assert(a > 0, "amount must be positive.");

        int64_t p = strtoll(strs.at(1).c_str(), nullptr, 10);
        eosio_assert(p >= 0 && p <= 18, "precision must be 0 ~ 18.");

        string sym = strs.at(2);
        eosio_assert(!sym.empty(), "symbol can't be empty.");

        a = a * pow(10, p);
        asset balance {a, ::eosio::string_to_symbol(p, sym.c_str())};
        dispatch_inline(_self, N(create),
                {permission_level(_self, N(active))}, std::make_tuple(_self, balance));

        dispatch_inline(_self, N(issue),
                {permission_level(_self, N(active))}, std::make_tuple(from, balance, string("issue new token")));
    }

    // It is allowed to transfer EOS or other tokens in eosio.token to _self.
}

#define EOSIO_ABI_EX( TYPE, MEMBERS ) \
extern "C" { \
    void apply( uint64_t receiver, uint64_t code, uint64_t action ) { \
        auto self = receiver; \
        if( action == N(onerror)) { \
            /* onerror is only valid if it is for the "eosio" code account and authorized by "eosio"'s "active permission */ \
            eosio_assert(code == N(eosio), "onerror action's are only valid from the \"eosio\" system account"); \
        } \
        if((code == receiver )|| (code == N(eosio.token) && action == N(transfer))) { \
            TYPE thiscontract( self ); \
            thiscontract.setCode(code); \
            switch( action ) { \
                EOSIO_API( TYPE, MEMBERS ) \
            } \
         /* does not allow destructor of thiscontract to run: eosio_exit(0); */ \
        } \
    } \
} \

EOSIO_ABI_EX( eosdactoken, (transfer)(create)(issue)(transferfee)(approve)(transferfrom)(balanceof)(allowance)(totalsupply)(copystates))
