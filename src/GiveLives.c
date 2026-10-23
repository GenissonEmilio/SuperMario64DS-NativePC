extern signed char data_0209f2f4;

#ifdef _MSC_VER
/* host only: the port's KeepLives setting (hal/char_save.cpp) writes the new count to its side file. Null on the DS side and in targets
   without the hook. */
void (*port_lives_note_hook)(int lives) = 0;
#endif

void GiveLives(int delta)
{
    int v = data_0209f2f4 + delta;
    if (v < 0)
        v = 0;
    else if (v > 0x64)
        v = 0x64;
    data_0209f2f4 = (signed char)v;
#ifdef _MSC_VER
    if (port_lives_note_hook)
        port_lives_note_hook(v);
#endif
}
