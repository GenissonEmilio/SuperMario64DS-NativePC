// Gate-7-only bridge: the C reference to dExtFrameCtrl_c::SetFlags forwards to
// the MSVC method definition. Lives apart from gx_upload_bridge because
// only targets that carry the gate-7 slice have the method to forward to.
struct dExtFrameCtrl_c {
    void SetFlags(int flags);
};
extern "C" void _ZN15dExtFrameCtrl_c8SetFlagsEi(void *self, int flags)
{
    ((dExtFrameCtrl_c *)self)->SetFlags(flags);
}
