#![allow(dead_code)]

extern crate rand;
#[macro_use] extern crate lazy_static;

#[macro_use] pub mod macros;
pub mod board;
pub mod bitboard;
pub mod eval;
pub mod movement;
pub mod movegen;
pub mod options;
pub mod perft;
pub mod position;
pub mod score;
pub mod search;
pub mod transposition;
pub mod uci;

fn main() {
    println!("Daydreamer {} ({}), by Aaron Becker",
             env!("CARGO_PKG_VERSION"),
             include_str!(concat!(env!("OUT_DIR"), "/version.rs")));
    bitboard::initialize();
    position::initialize();
    ::options::set_multi_pv(1);
    let mut search_data = search::SearchData::new();

    // Treat each argument as a file containing uci commands.
    for arg in ::std::env::args().skip(1) {
        uci::read_stream(&mut search_data, Some(arg.to_string()));
    }
    // Read from stdin.
    uci::read_stream(&mut search_data, None);
}
