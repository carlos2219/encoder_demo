#ifndef PID_H
#define PID_H

typedef struct {
    /* PID coefficients */
    float Kp;
    float Ki;
    float Kd;

    /* Derivative low-pass filter time constant */
	float tau;

    /* Output limits */
    float limMin;
    float limMax;

    /* Integrator limits */
    float limMinInt;    
    float limMaxInt;

    /* Sample time (in seconds) */
	float T;

    /* Controller "memory" */
    float integrator;
    float prevError;
    float differentiator;
    float prevMeasurement;
    
    /* Controller output */
    float out;
} PIDController;

void PID_Init(PIDController *pid);
float PID_Compute(PIDController *pid, float setpoint, float measurement);

#endif /* PID_H */