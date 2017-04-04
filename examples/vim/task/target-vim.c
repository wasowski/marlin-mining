// TODO: BEGIN fc9e0bb
static char *(p_bg_values[]) = {"light", "dark", NULL};
static char *(p_nf_values[]) = {"bin", "octal", "hex", "alpha", NULL};
#ifdef FEAT_CMDL_COMPL
static char *(p_clcot_values[]) = {"menu", "menuone", "longest", "noinsert", "noselect", NULL};
#endif /* defined(FEAT_CMDL_COMPL) */
static char *(p_ff_values[]) = {FF_UNIX, FF_DOS, FF_MAC, NULL};

options[] =
#ifdef OS2
(char_u *)"/c",
#else
(char_u *)"-c",
#endif /* defined(OS2) */
// TODO: END fc9e0bb

// TODO: BEGIN ca7753f
#if defined(FEAT_CMDL_COMPL)
/* Don't allow recursive cmdline mode when busy with completion. */
#endif /* defined(FEAT_CMDL_COMPL) */
#if defined(FEAT_CMDL_COMPL)
if (clpum_compl_started || clpum_compl_busy || clpum_visible())
#endif /* defined(FEAT_CMDL_COMPL) */
#if defined(FEAT_CMDL_COMPL)
{
#endif /* defined(FEAT_CMDL_COMPL) */
#if defined(FEAT_CMDL_COMPL)
EMSG(_(e_secure));
#endif /* defined(FEAT_CMDL_COMPL) */
#if defined(FEAT_CMDL_COMPL)
return NULL;
#endif /* defined(FEAT_CMDL_COMPL) */
#if defined(FEAT_CMDL_COMPL)
}
#endif /* defined(FEAT_CMDL_COMPL) */
#if defined(FEAT_CMDL_COMPL)
clpum_compl_clear();    /* clear stuff for clpum */
#endif /* defined(FEAT_CMDL_COMPL) */
#if defined(FEAT_CMDL_COMPL)

#endif /* defined(FEAT_CMDL_COMPL) */
switch (c) {
#ifdef FEAT_CMDL_COMPL
if (clpum_visible())
showmode();
#endif /* defined(FEAT_CMDL_COMPL) */
#ifdef FEAT_CMDHIST
i = hiscnt;
#endif /* defined(FEAT_CMDHIST) */
beep_flush();
}
// TODO: END ca7753f
