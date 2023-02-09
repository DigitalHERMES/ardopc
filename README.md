# ardopc
ARDOP (Amateur Radio Digital Open Protocol) TNC and SDR modem implementation by John Wiseman (GM8BPQ). Unofficial repository.

# Building
`git clone https://github.com/knowmercy/ardopc`
`cd ardopc/ardop1ofdm`
`make`

# Current Setup

This is outlined because ardop does not run in isolation. It interfaces directly with other software. Protocol versions and bugfixes in each of the supporting software programs can be derived from this. 

Radio is an ic-705 connected via USB
 - Radio Settings:
    - Menu -> Set -> Connectors -> USB MOD Level = 50%
    - Menu -> Set -> Connectors -> DATA OFF MOD = MIC, USB
    - Menu -> Set -> Connectors -> DATA MOD = USB


Built ardopc from my git repo
  - cd ardop1ofdm && make
  - ~/.asoundrc
    - pcm.ARDOP {type rate slave {pcm "hw:2,0" rate 48000}}
  - ./ardop1ofdm


Built hamlib from source
  -   wget https://sourceforge.net/projects/hamlib/files/hamlib/4.2/hamlib-4.2.tar.gz
  - tar xzvf hamlib-4.2.tar.gz
  - cd hamlib-4.2
  - ./configure --prefix=/usr/local --enable-static
  - make
  - sudo make install
  - /usr/bin/rigctld -m 3085 -r /dev/ttyACM0 -s 115200


Built pat from source
  - git clone https://github.com/la5nta/pat.git
  - cd pat
  - go build
  - ./pat http
