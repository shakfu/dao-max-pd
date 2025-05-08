#include "m_pd.h"

#include <math.h>

/* The object structure *******************************************************/
typedef struct _cartopol {
    t_object obj;
    t_float x_f;
} t_cartopol;

/* The arguments/inlets/outlets/vectors indexes *******************************/
enum ARGUMENTS { NONE };
enum INLETS { I_REAL, I_IMAG, NUM_INLETS };
enum OUTLETS { O_MAGN, O_PHASE, NUM_OUTLETS };
enum DSP { PERFORM, OBJECT,
           INPUT_REAL, INPUT_IMAG, OUTPUT_MAGN, OUTPUT_PHASE,
           VECTOR_SIZE, NEXT };

/* The class pointer **********************************************************/
static t_class *cartopol_class;

/* Function prototypes ********************************************************/
void *cartopol_common_new(t_cartopol *x, short argc, t_atom *argv);
void cartopol_free(t_cartopol *x);

void cartopol_dsp(t_cartopol *x, t_signal **sp, short *count);
t_int *cartopol_perform(t_int *w);


/* Function prototypes ********************************************************/
void *cartopol_new(t_symbol *s, short argc, t_atom *argv);

/* The 'initialization' routine ***********************************************/
#ifdef WIN32
__declspec(dllexport) void cartopol_tilde_setup(void);
#endif
void cartopol_tilde_setup(void)
{
    /* Initialize the class */
    cartopol_class = class_new(gensym("cartopol~"),
                               (t_newmethod)cartopol_new,
                               (t_method)cartopol_free,
                               sizeof(t_cartopol), 0, A_GIMME, 0);

    /* Specify signal input, with automatic float to signal conversion */
    CLASS_MAINSIGNALIN(cartopol_class, t_cartopol, x_f);

    /* Bind the DSP method, which is called when the DACs are turned on */
    class_addmethod(cartopol_class, (t_method)cartopol_dsp, gensym("dsp"), 0);

    /* Bind the object-specific methods */
    // nothing

    /* Print message to Max window */
    post("cartopol~ • External was loaded");
}

/* The 'new instance' routine *************************************************/
void *cartopol_new(t_symbol *s, short argc, t_atom *argv)
{
    /* Instantiate a new object */
    t_cartopol *x = (t_cartopol *)pd_new(cartopol_class);

    return cartopol_common_new(x, argc, argv);
}


/* The common 'new instance' routine ******************************************/
void *cartopol_common_new(t_cartopol *x, short argc, t_atom *argv)
{
    /* Create inlets */
    inlet_new(&x->obj, &x->obj.ob_pd, gensym("signal"), gensym("signal"));

    /* Create signal outlets */
    outlet_new(&x->obj, gensym("signal"));
    outlet_new(&x->obj, gensym("signal"));

    /* Print message to Max window */
    post("cartopol~ • Object was created");

    /* Return a pointer to the new object */
    return x;
}

/* The 'free instance' routine ************************************************/
void cartopol_free(t_cartopol *x)
{
    /* Print message to Max window */
    post("cartopol~ • Memory was freed");
}


/* The 'DSP' method ***********************************************************/
void cartopol_dsp(t_cartopol *x, t_signal **sp, short *count)
{
    /* Attach the object to the DSP chain */
    dsp_add(cartopol_perform, NEXT-1, x,
            sp[0]->s_vec,
            sp[1]->s_vec,
            sp[2]->s_vec,
            sp[3]->s_vec,
            sp[0]->s_n);

    /* Print message to Max window */
    post("cartopol~ • Executing 32-bit perform routine");
}

/* The 'perform' routine ******************************************************/
t_int *cartopol_perform(t_int *w)
{
    /* Copy the object pointer */
    // t_cartopol *x = (t_cartopol *)w[OBJECT];

    /* Copy signal pointers */
    t_float *input_real = (t_float *)w[INPUT_REAL];
    t_float *input_imag = (t_float *)w[INPUT_IMAG];
    t_float *output_magn = (t_float *)w[OUTPUT_MAGN];
    t_float *output_phase = (t_float *)w[OUTPUT_PHASE];

    /* Copy the signal vector size */
    t_int n = w[VECTOR_SIZE];

    /* Perform the DSP loop */
    int framesize;
    framesize = (int)(n / 2) + 1;

    float local_real;
    float local_imag;

    for (int ii = 0; ii < framesize; ii++) {
        local_real = input_real[ii];
        local_imag = (ii == 0 || ii == framesize-1) ? 0.0 : input_imag[ii];

        output_magn[ii] = hypotf(local_real, local_imag);
        output_phase[ii] = -atan2(local_imag, local_real);
    }

    for (int ii = framesize; ii < n; ii++) {
        output_magn[ii] = 0.0;
        output_phase[ii] = 0.0;
    }


    /* Return the next address in the DSP chain */
    return w + NEXT;
}
