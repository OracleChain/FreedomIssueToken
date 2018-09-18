rm -rf publictokens.wasm
rm -rf publictokens.wast
rm -rf publictokens.abi

eosiocpp -o publictokens.wast publictokens.cpp
eosiocpp -g publictokens.abi publictokens.hpp