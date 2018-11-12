# Network Countdown

Awhile ago I found myself watching a movie over a video chat with my (at the moment) long-distance girlfriend. To synchronize the movie, we had to use our voices to do a countdown. It felt a little silly and imprecise to do a countdown with our voices in this way. To let computers do the work, I wrote this program.

## Usage

### Running the Client

### Running the Server

### Dependencies

Doing the countdown over the speakers requires text-to-voice command-line software. If you are using a mac, the `say` command is built in and you don't need to install anything. If you are using an ubuntu-style linux distribution, the `gnustep-gui-runtime` package provides a `say` command . Other platforms have not been considered so far, but if you would like to use this with a different platform send me an email at seewalker.120@gmail.com and I will work something out.

## Plans

- Use ifdef to allow no testing and no privacy. (no testing should be default. privacy should be default if the relevant libraries can be found).

- Use openssl for privacy.
