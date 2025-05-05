/* Common header files ********************************************************/
#include "mirror~common.h"

/* The 'DSP/perform' arguments list *******************************************/
enum DSP {PERFORM, OBJECT, INPUT_VECTOR, OUTPUT_VECTOR, VECTOR_SIZE, NEXT};

#ifdef TARGET_IS_MAX

void mirror_dsp64(t_mirror* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags)
{
    post("mirror~ • Executing 64-bit perform routine");

    object_method(dsp64, gensym("dsp_add64"), x, mirror_perform64, 0, NULL);
}

void mirror_perform64(t_mirror* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
{
    t_double* inL = ins[0];
    t_double* outL = outs[0];
    int n = sampleframes;

    while (n--) {
        *outL++ = GAIN * *inL++;
    }
}


#elif TARGET_IS_PD

void mirror_dsp(t_mirror *x, t_signal **sp, short *count)
{
    /* Attach the object to the DSP chain */
    dsp_add(mirror_perform, NEXT-1, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
    
    /* Print message to Max window */
    post("mirror~ • Executing 32-bit perform routine");
}


t_int *mirror_perform(t_int *w)
{
    /* Copy the object pointer */
    t_mirror *x = (t_mirror *)w[OBJECT];
    
    /* Copy signal pointers */
    t_float *in = (t_float *)w[INPUT_VECTOR];
    t_float *out = (t_float *)w[OUTPUT_VECTOR];
    
    /* Copy the signal vector size */
    t_int n = w[VECTOR_SIZE];
    
    /* Perform the DSP loop */
    while (n--) {
        *out++ = GAIN * *in++;
    }
    
    /* Return the next address in the DSP chain */
    return w + NEXT;
}

/******************************************************************************/
#endif