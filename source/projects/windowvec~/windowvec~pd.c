#include "m_pd.h"

#include <math.h>
#include <stdlib.h>

/* The global variables
 * *******************************************************/
#define TWOPI 6.2831853071796

/* The object structure
 * *******************************************************/
typedef struct _windowvec {
    t_object obj;
    t_float x_f;

    float fs;

    float* window;
    int vecsize;
} t_windowvec;

/* The arguments/inlets/outlets/vectors indexes
 * *******************************/
enum ARGUMENTS { NONE };
enum INLETS { I_INPUT, NUM_INLETS };
enum OUTLETS { O_OUTPUT, NUM_OUTLETS };
enum DSP { PERFORM, OBJECT, INPUT1, OUTPUT1, VECTOR_SIZE, NEXT };

/* The class pointer
 * **********************************************************/
static t_class* windowvec_class;

/* Function prototypes
 * ********************************************************/
void* windowvec_common_new(t_windowvec* x, short argc, t_atom* argv);
void windowvec_free(t_windowvec* x);
void windowvec_dsp(t_windowvec* x, t_signal** sp, short* count);
t_int* windowvec_perform(t_int* w);
/******************************************************************************/

/* Function prototypes
 * ********************************************************/
void* windowvec_new(t_symbol* s, short argc, t_atom* argv);

/* The 'initialization' routine
 * ***********************************************/
#ifdef WIN32
__declspec(dllexport) void windowvec_tilde_setup(void);
#endif
void windowvec_tilde_setup(void)
{
    /* Initialize the class */
    windowvec_class = class_new(
        gensym("windowvec~"), (t_newmethod)windowvec_new,
        (t_method)windowvec_free, sizeof(t_windowvec), 0, A_GIMME, 0);

    /* Specify signal input, with automatic float to signal conversion */
    CLASS_MAINSIGNALIN(windowvec_class, t_windowvec, x_f);

    /* Bind the DSP method, which is called when the DACs are turned on */
    class_addmethod(windowvec_class, (t_method)windowvec_dsp, gensym("dsp"),
                    0);

    /* Bind the object-specific methods */
    // nothing

    /* Print message to Max window */
    post("windowvec~ • External was loaded");
}

/* The 'new instance' routine
 * *************************************************/
void* windowvec_new(t_symbol* s, short argc, t_atom* argv)
{
    /* Instantiate a new object */
    t_windowvec* x = (t_windowvec*)pd_new(windowvec_class);

    return windowvec_common_new(x, argc, argv);
}

/* The common 'new instance' routine
 * ******************************************/
void* windowvec_common_new(t_windowvec* x, short argc, t_atom* argv)
{
    /* Create inlets */
    // nothing - Pd gives one by default

    /* Create signal outlets */
    outlet_new(&x->obj, gensym("signal"));

    /* Initialize some state variables */
    x->fs = sys_getsr();

    x->window = NULL;
    x->vecsize = 128;

    /* Print message to Max window */
    post("windowvec~ • Object was created");

    /* Return a pointer to the new object */
    return x;
}

/* The 'free instance' routine
 * ************************************************/
void windowvec_free(t_windowvec* x)
{
    /* Free allocated dynamic memory */
    if (x->window != NULL) {
        free(x->window);
    }

    /* Print message to Max window */
    post("windowvec~ • Memory was freed");
}

/* The 'DSP' method
 * ***********************************************************/

void windowvec_dsp(t_windowvec* x, t_signal** sp, short* count)
{
    if (x->vecsize != sp[0]->s_n) {
        x->vecsize = sp[0]->s_n;

        int bytesize = x->vecsize * sizeof(float);
        if (x->window == NULL) {
            x->window = (float*)malloc(bytesize);
        } else {
            x->window = (float*)realloc(x->window, bytesize);
        }

        for (int ii = 0; ii < x->vecsize; ii++) {
            x->window[ii] = -0.5 * cos(TWOPI * ii / (float)x->vecsize) + 0.5;
        }
    }

    /* Attach the object to the DSP chain */
    dsp_add(windowvec_perform, NEXT - 1, x, sp[0]->s_vec, sp[1]->s_vec,
            sp[0]->s_n);

    /* Print message to Max window */
    post("windowvec~ • Executing 32-bit perform routine");
}

/* The 'perform' routine
 * ******************************************************/
t_int* windowvec_perform(t_int* w)
{
    /* Copy the object pointer */
    t_windowvec* x = (t_windowvec*)w[OBJECT];

    /* Copy signal pointers */
    t_float* input = (t_float*)w[INPUT1];
    t_float* output = (t_float*)w[OUTPUT1];

    /* Copy the signal vector size */
    t_int n = w[VECTOR_SIZE];

    /* Load state variables */
    float* window = x->window;
    // float vecsize = x->vecsize;

    /* Perform the DSP loop */
    for (int ii = 0; ii < n; ii++) {
        output[ii] = input[ii] * window[ii];
    }

    /* Return the next address in the DSP chain */
    return w + NEXT;
}
