<h1 align="center">
  <img height="130" src="https://github.com/marlinprotocol/OpenWeaver/blob/master/img/OpenWeaver_Black.jpg?raw=true"/>
</h1>

<!-- <h2 align="center">FIBRE for altcoins</h2> -->
The codebase is a pretty standard C++/CMake project. It has submodules, so be sure to checkout/clone recursively to include them.

## Building
1. `mkdir build && cd build` to create a build directory
2. `cmake .. -DCMAKE_BUILD_TYPE=Release && make -j8` to build

## Core network

- Beacon - Built at `./beacon/server`
- Master - Built at `./relay/masterexec`
- Relay - Built at `./relay/relayexec`

## Integrations

- Ethereum - A bridge between eth nodes and the core network is built at `./integrations/eth/onramp_eth`

## Support

| Topic  | Link  |
|---|---|
|Development   | <a href="https://discord.gg/pdQZyyy">Discord</a>  |
|Research   | <a href="https://research.marlin.pro">Discourse</a>  |
|Bug(s)  | <a href="https://github.com/marlinprotocol/OpenWeaver/issues/new">Issues</a>  |
|Memes  | <a href="https://t.me/marlinprotocol">Telegram</a>  |
|Spam  | info@marlin.pro  |
