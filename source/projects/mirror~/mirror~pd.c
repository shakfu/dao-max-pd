#include "m_pd.h"

/* The global variables
 * *******************************************************/
#define GAIN 0.1

/* The object structure
 * *******************************************************/
typedef struct _mirror {
    t_object obj;
    t_float x_f;
} t_mirror;

/* The class pointer
 * **********************************************************/
static t_class* mirror_class;

/* Function prototypes
 * ********************************************************/
void mirror_dsp(t_mirror* x, t_signal** sp, short* count);
t_int* mirror_perform(t_int* w);
void* mirror_new(void);
void mirror_free(t_mirror* x);

/* The 'initialization' routine
 * ***********************************************/
#ifdef WIN32
__declspec(dllexport) void mirror_tilde_setup(void);
#endif
void mirror_tilde_setup(void)
{
    /* Initialize the class */
    mirror_class = class_new(gensym("mirror~"), (t_newmethod)mirror_new,
                             (t_method)mirror_free, sizeof(t_mirror), 0, 0);

    /* Specify signal input, with automatic float to signal conversion */
    CLASS_MAINSIGNALIN(mirror_class, t_mirror, x_f);

    /* Bind the DSP method, which is called when the DACs are turned on */
    class_addmethod(mirror_class, (t_method)mirror_dsp, gensym("dsp"), 0);

    /* Print message to Max window */
    post("mirror~ • External was loaded");
}

/* The 'new instance' routine
 * *************************************************/
void* mirror_new(void)
{
    /* Instantiate a new object */
    t_mirror* x = (t_mirror*)pd_new(mirror_class);

    /* Create signal inlets */
    // Pd creates one by default

    /* Create signal outlets */
    outlet_new(&x->obj, gensym("signal"));

    /* Print message to Max window */
    post("mirror~ • Object was created");

    /* Return a pointer to the new object */
    return x;
}

/* The 'free instance' routine
 * ************************************************/
void mirror_free(t_mirror* x)
{
    // Nothing to free

    /* Print message to Max window */
    post("mirror~ • Memory was freed");
}

/* The 'DSP/perform' arguments list
 * *******************************************/
enum DSP { PERFORM, OBJECT, INPUT_VECTOR, OUTPUT_VECTOR, VECTOR_SIZE, NEXT };

void mirror_dsp(t_mirror* x, t_signal** sp, short* count)
{
    /* Attach the object to the DSP chain */
    dsp_add(mirror_perform, NEXT - 1, x, sp[0]->s_vec, sp[1]->s_vec,
            sp[0]->s_n);

    /* Print message to Max window */
    post("mirror~ • Executing 32-bit perform routine");
}


t_int* mirror_perform(t_int* w)
{
    /* Copy the object pointer */
    t_mirror* x = (t_mirror*)w[OBJECT];

    /* Copy signal pointers */
    t_float* in = (t_float*)w[INPUT_VECTOR];
    t_float* out = (t_float*)w[OUTPUT_VECTOR];

    /* Copy the signal vector size */
    t_int n = w[VECTOR_SIZE];

    /* Perform the DSP loop */
    while (n--) {
        *out++ = GAIN * *in++;
    }

    /* Return the next address in the DSP chain */
    return w + NEXT;
}
