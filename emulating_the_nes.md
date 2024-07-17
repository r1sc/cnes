** WIP ** ** WIP ** ** WIP ** ** WIP ** ** WIP ** ** WIP ** ** WIP ** ** WIP ** ** WIP ** ** WIP ** 
** WIP ** ** WIP ** ** WIP ** ** WIP ** ** WIP ** ** WIP ** ** WIP ** ** WIP ** ** WIP ** ** WIP ** 

# Emulating the NES

Let me start off with a big caveat - I'm no expert on the NES. I am no expert on digital signal processing,
nor am I an expert on emulating systems. These concepts are not new to me, I've dabbled in them
before on various projects, but you should not expect the following text to be perfectly accurate. This is my understanding
as far as it goes and the techniques and details that I describe here are what worked for me when designing
my own emulator. Consult the NesDev wiki and forums <----->.

## The Audio Processing Unit (APU)

The NES audio subsystem, the APU is what generates sounds and music on the NES. It's physically part of the
CPU chip, but it's not part of the CPU subsystem. In my emulator I'm treating the APU as a distinct device on
the bus.

The APU is clocked every second CPU clock. Since the CPU is clocked every third PPU clock, you can say that the
APU then is clocked every 6th PPU clock (3 PPU clocks * 2 CPU clocks). Every APU clock, the APU generates a new
output sample.

From this we can calculate the frequency at which the APU runs, or its *sample rate*.
We know that the PPU makes 262 scanlines, each consisting of 340 dots. In one second it does this 60 times.

> The PAL nes does it 50 times per second, but has more scanlines, we'll just consider the NTSC NES for now.

The frequency of the PPU is thus, 262 scanlines * 340 dots * 60 frames/sec = 5 344 800 Hz, or ~5,34 Mhz.
Now since the APU runs at 1/6th of the speed of the PPU, the APU has a frequency of 890 800 Hz.

This sample rate is probably too high for your soundcard to handle, so we need to downsample.
In my emulator I considered a few different ways.

> Some sources claim that the sample rate of the APU is 44100 Hz. This is just plain wrong and a misunderstanding
of what is what. Other sources claim that the sample rate is 1,8 Mhz. I don't know how they come up with this
number, but I guess they mean the bandwidth of the NTSC signal. That's not the sample rate of the APU, since there
are not 1,8M distinct samples per second, but merely the sample rate of the receiving TV set.

One way is to run your audio output in parallell with the NES and grab samples from the APU as you need them.
Often you accomplish this by running the output in a different thread, pushing samples into a buffer and
feeding these buffers to your audio driver. This is very flexible because you can run the driver at which
sample rate you need, say 44100 Hz which is a popular frequency, but it's easy to drift off the source signal
and introduce audio latency.

Another way, which is how it works in my emulator, is that I push one sample from the APU per *scanline*.
This gives a sample rate of 262 scanlines * 60 Hz = 15 720 Hz. That's not a lot of detail, but considering
the waveforms involved (square wave + triangle + crude PCM), this is pretty OK anyway.

In my emulator, the APU calls back to the host once per scanline for the audio engine to fill its buffer.

One thing you probably need to do, unless you have a very high sample rate, is to low-pass filter the audio.
The kind of waveforms the NES plays, esp. square waves, have lots of harmonics at the zero passes, i.e. where
the signal traverses from low to high or from high to low. This results in very unpleasant ringing unless the
resulting high frequencies are filtered out.
A low-pass filter lets lower frequencies pass and blocks higher frequencies. It's like a spring, basically.
Quick changes have no dramatic effect on the spring, but changes over time will compress or contract the spring.

In my APU I have a super crude 1-pole filter like this:

`sample_out += ((new_sample - sample_out) / 64);`

Here we take the delta of the new sample and the previous filtered sample. We take a fraction of this delta and 
add that back to the filtered sample. So it works as a spring, or as a capacitor does in electronics. 
A higher divider results in a wider spectrum of filtered frequencies and a lower divider gives more detail but
let's higher frequencies pass. You'll need to experiment to find a value that sounds nice.
