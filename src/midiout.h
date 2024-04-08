
#include "doomtype.h"

void MIDI_SendShortMsg(byte status, byte channel, byte param1, byte param2);

void MIDI_SendLongMsg(const byte *message, unsigned int length);

int MIDI_CountDevices(void);

const char *MIDI_GetDeviceName(int device);

boolean MIDI_OpenDevice(int device);

void MIDI_CloseDevice(void);
