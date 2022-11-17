## BM1397 protocol
notes from sniffing BM1397 communication and trying to match it up with cgminer

It looks like this setting up the BM1397:
```
TX: 55 AA 52 05 00 00 0A // also "chippy"??
CMD.RX: AA 55 13 97 18 00 00 00 06
TX: 55 AA 53 05 00 00 03 //chain inactive
TX: 55 AA 53 05 00 00 03 //chain inactive
TX: 55 AA 53 05 00 00 03 //chain inactive
TX: 55 AA 40 05 00 00 1C //"chippy"??
TX: 55 AA 51 09 00 80 00 00 00 00 1C //init1
TX: 55 AA 51 09 00 84 00 00 00 00 11 //init2
TX: 55 AA 51 09 00 20 00 00 00 01 02 //init3
TX: 55 AA 51 09 00 3C 80 00 80 74 10 //init4
TX: 55 AA 51 09 00 14 00 00 00 00 1C //set_ticket
TX: 55 AA 51 09 00 68 C0 70 01 11 00 //init5
TX: 55 AA 51 09 00 68 C0 70 01 11 00 //init5 (2x)
TX: 55 AA 51 09 00 28 06 00 00 0F 18 //init6
TX: 55 AA 51 09 00 18 00 00 7A 31 15 //baudrate
TX: 55 AA 51 09 00 70 0F 0F 0F 00 19 //prefreq
TX: 55 AA 51 09 00 70 0F 0F 0F 00 19 //prefreq
TX: 55 AA 51 09 00 08 40 A0 02 25 16 //freqbuf
TX: 55 AA 51 09 00 08 40 A0 02 25 16 //freqbuf
```

Sending work to the BM1397 looks like this;
```
TX: 55 AA 21 96 04 04 00 00 00 00 72 E7 07 17 A9 81 51 63 EE D6 E5 43 35 3F 14 92 56 25 54 19 4E 41 31 08 E5 D7 89 4A C8 13 50 A4 48 05 B8 0E E2 E4 83 95 F8 C1 15 8D EC 07 D8 B8 AE CA DE E6 35 8C 3E E9 1C 57 5D 99 A7 52 95 DC D8 08 7B 7A D
```

How does this big work field break down?

`55 AA` ->
`21` -> always 21. like bitcoin, always 21 million coins?
`96` -> length
`04` -> jobID
`04` -> midstates
`00 00 00 00` -> always zero. Is this the starting nonce?
`72 E7 07 17` -> nbits
`A9 81 51 63` -> ntime. if you flip the endianess, this is unixtimestamp for Thu Oct 20 2022 17:13:13 GMT. boom!
`EE D6 E5 43` -> last 4 bytes of the merkle root
`35 3F 14 92 56 25 54 19 4E 41 31 08 E5 D7 89 4A C8 13 50 A4 48 05 B8 0E E2 E4 83 95 F8 C1 15 8D` -> midstate computed from the first 64 bytes of the header
`EC 07 D8 B8 AE CA DE E6 35 8C 3E E9 1C 57 5D 99 A7 52 95 DC D8 08 7B 7A D` -> ?? another midstate?

when the BM1397 finds a nonce that makes a hash below the difficulty (which difficulty is that?) it responds with;
```
AA 55 00 03 EA 0F 02 20 9C
```
this means the Nonce 0003ea0f was found for JobID 20

If the BM1397 _doesn't_ find a good nonce in the whole nonce space, it just goes off into lalaland. It's up to the miner to keepm track of what the ASIC is doing and send it more work if this happens. If you look at the hashing frequency you should be able to time this.
