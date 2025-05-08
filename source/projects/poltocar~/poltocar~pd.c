#include "m_pd.h"

#include <math.h>

/* The object structure *******************************************************/
typedef struct _poltocar {
    t_object obj;
    t_float x_f;
} t_poltocar;

/* The arguments/inlets/outlets/vectors indexes *******************************/
enum ARGUMENTS { NONE };
enum INLETS { I_MAGN, I_PHASE, NUM_INLETS };
enum OUTLETS { O_REAL, O_IMAG, NUM_OUTLETS };
enum DSP { PERFORM, OBJECT,
           INPUT_MAGN, INPUT_PHASE, OUTPUT_REAL, OUTPUT_IMAG,
           VECTOR_SIZE, NEXT };

/* The class pointer **********************************************************/
static t_class *poltocar_class;

/* Function prototypes ********************************************************/
void *poltocar_common_new(t_poltocar *x, short argc, t_atom *argv);
void poltocar_free(t_poltocar *x);

void poltocar_dsp(t_poltocar *x, t_signal **sp, short *count);
t_int *poltocar_perform(t_int *w);

/******************************************************************************/

/* Function prototypes ********************************************************/
void *poltocar_new(t_symbol *s, short argc, t_atom *argv);

/* The 'initialization' routine ***********************************************/
#ifdef WIN32
__declspec(dllexport) void poltocar_tilde_setup(void);
#endif
void poltocar_tilde_setup(void)
{
    /* Initialize the class */
    poltocar_class = class_new(gensym("poltocar~"),
                               (t_newmethod)poltocar_new,
                               (t_method)poltocar_free,
                               sizeof(t_poltocar), 0, A_GIMME, 0);

    /* Specify signal input, with automatic float to signal conversion */
    CLASS_MAINSIGNALIN(poltocar_class, t_poltocar, x_f);

    /* Bind the DSP method, which is called when the DACs are turned on */
    class_addmethod(poltocar_class, (t_method)poltocar_dsp, gensym("dsp"), 0);

    /* Bind the object-specific methods */
    // nothing

    /* Print message to Max window */
    post("poltocar~ • External was loaded");
}

/* The 'new instance' routine *************************************************/
void *poltocar_new(t_symbol *s, short argc, t_atom *argv)
{
    /* Instantiate a new object */
    t_poltocar *x = (t_poltocar *)pd_new(poltocar_class);

    return poltocar_common_new(x, argc, argv);
}

/* The common 'new instance' routine ******************************************/
void *poltocar_common_new(t_poltocar *x, short argc, t_atom *argv)
{
    /* Create inlets */
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));

    /* Create signal outlets */
    outlet_new(&x->obj, gensym("signal"));
    outlet_new(&x->obj, gensym("signal"));

    /* Print message to Max window */
    post("poltocar~ • Object was created");

    /* Return a pointer to the new object */
    return x;
}

/* The 'free instance' routine ************************************************/
void poltocar_free(t_poltocar *x)
{
    /* Print message to Max window */
    post("poltocar~ • Memory was freed");
}

/* The 'DSP' method ***********************************************************/
void poltocar_dsp(t_poltocar *x, t_signal **sp, short *count)
{
    /* Attach the object to the DSP chain */
    dsp_add(poltocar_perform, NEXT-1, x,
            sp[0]->s_vec,
            sp[1]->s_vec,
            sp[2]->s_vec,
            sp[3]->s_vec,
            sp[0]->s_n);

    /* Print message to Max window */
    post("poltocar~ • Executing 32-bit perform routine");
}

/* The 'perform' routine ******************************************************/
t_int *poltocar_perform(t_int *w)
{
    /* Copy the object pointer */
    // t_poltocar *x = (t_poltocar *)w[OBJECT];

    /* Copy signal pointers */
    t_float *input_magn = (t_float *)w[INPUT_MAGN];
    t_float *input_phase = (t_float *)w[INPUT_PHASE];
    t_float *output_real = (t_float *)w[OUTPUT_REAL];
    t_float *output_imag = (t_float *)w[OUTPUT_IMAG];

    /* Copy the signal vector size */
    t_int n = w[VECTOR_SIZE];

    /* Perform the DSP loop */
    int framesize;
    framesize = (int)(n / 2) + 1;

    float local_real;
    float local_imag;

    for (int ii = 0; ii < framesize; ii++) {
        local_real = input_magn[ii] * cos(input_phase[ii]);
        local_imag = -input_magn[ii] * sin(input_phase[ii]);

        output_real[ii] = local_real;
        output_imag[ii] = (ii == 0 || ii == framesize-1) ? 0.0 : local_imag;
    }
        for (int ii = framesize; ii < n; ii++) {
            output_real[ii] = 0.0;
            output_imag[ii] = 0.0;
        }

    /* Return the next address in the DSP chain */
    return w + NEXT;
}
