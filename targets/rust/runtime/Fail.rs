// Stops the program with a message, as polyir's `fail` does.
pub fn fail(message: &str) -> ! {
    eprintln!("trap: {}", message);
    std::process::exit(1)
}
