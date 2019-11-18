ARDOPOFDM is an experimental extension to ARDOP2. The higher speed PSK and QAM modes of ARDOP2 are replaced with OFDM modes. There are two bandwidths, 500 and 2500 and 4 modes within each bandwidth (2PSK, 4PSK 8PSK and 16PSK or 16QAM). The code uses a modulation rate and carrier spacing of 55.555 Hz. The 500 mode uses 9 carriers and the 2500 mode 43 carriers. There isn't a 200 wide version.

The code runs on Windows, Linux and my Teensy TNC.

ARDOPOFDM has one new configuration parameter, ENABLEOFDM, with a default of TRUE. Apart from the new data frames it has two new CONREQ frames, OCONREQ500 and OCONREQ2500 and a new ACK, OFDMACK. If ENABLEOFDM is set it will send and accept the OCONREQ frames and the OFDM data frames. For compatibility with ARDOP2 stations if it receives a normal CONREQ frame it will disable the use of the OFDM data frames. If ENABLEOFDM is set to FALSE it will use normal CONREQ frames and reject OCONREQ, so it can connect to ARDOP2 stations. ARDOPOFDM acks each carrier separately, so only carriers that have failed to decode are repeated. If all carriers are ok a normal DataACK or DataACKHQ frame is sent, if not a slightly longer OFDMACK is sent which indicates which carriers need to be repeated.

I haven't been able to evaluate it under a wide range of radio conditions, but initial testing looks promising. Under ideal conditions it is a bit over twice as fast as normal ARDOP (about 80% of the speed of VARA). Data frame length is a little under 5 seconds. Under ideal conditions performance in 2500 mode is

16OFDM 80 bytes/carrier, 3440 bytes per frame, approx 4600 BPS Net
 8OFDM 60 bytes/carrier, 2580 bytes per frame, approx 3440 BPS Net
 4OFDM 40 bytes/carrier, 1720 bytes per frame, approx 2300 BPS Net
 2OFDM 19 bytes/carrier,  817 bytes per frame, approx 1100 BPS Net

For Comparison 16QAM.2500.100 (10 Carriers)

120 bytes/carrier, 1200 bytes per frame, approx 2225 BPS Net

As mentioned earlier this a experimental and modulation details and frame formats are likely to change as testing proceeds. It is really only suitable for people who a used to testing software. 

Software can be downloaded from

http://www.cantab.net/users/john.wiseman/Downloads/Beta/ARDOPOFDM.exe
http://www.cantab.net/users/john.wiseman/Downloads/Beta/ardopofdm
http://www.cantab.net/users/john.wiseman/Downloads/Beta/piardopofdm
http://www.cantab.net/users/john.wiseman/Downloads/Beta/TeensyProjects.zip


The software has only been tested with BPQ32/LinBPQ and Winlink Express in PTC Emulation mode. You need to install the latest Beta BPQ32 or linbpq to use it. It may work with RMS Express in normal mode and Trimode, but this hasn't been tested and may result in slow running or data corruption due to changes in flow control thresholds.

