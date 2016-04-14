
const uint32_t TCLK = 1;
const uint32_t TIOA = 2;
const uint32_t TIOB = 4;

uint32_t n_px = 128;
const int divider = 16;

bool active_modulation = false;

// Admin commands
#define MSG_IDENTIFY 0x49 // 'I'
#define MSG_IDENTITY 0x4A

#define MSG_SET_MODULATION 0x01
#define MSG_SET_NUM_PIXELS 0x02

char param[4];
float* param_f = (float*) param;
uint32_t* param_ui = (uint32_t*) param;


int cycle_length = 10000;
int pulse_length =  5000;
int eom_pulse_length = 10000;

// EOM output from Leica connected to TC2_0_TIOA : Digital Pin 5 : LINE TRIGGER  
// Connect TC2_1 to EOM to TC2_1: Digital Pin 3 : MUX OUT

void TC6_Handler()
{
  int state = REG_TC2_SR0;  
  int count = REG_TC2_RA0;

  eom_pulse_length = count;
  cycle_length = count / n_px;
  pulse_length = count / n_px / divider;

  setTimerRegisters();
}

int px = 0;
void TC7_Handler() // we finish one pixel
{
  int state = REG_TC2_SR1;  

  // Make sure we stop after n_px 

  px++;
  if (px == (n_px-1))
  {
    px = 0;
    REG_TC2_CMR2 |= TC_CMR_CPCSTOP;
  }
  elseif (active_modulation)
  {
    REG_TC2_CMR2 &= (!TC_CMR_CPCSTOP);
  }
}

void setTimerRegisters()
{
  if (active_modulation)
  {
    REG_TC2_RC1 = cycle_length;
    REG_TC2_RA1 = pulse_length;  
  } 
  else 
  {
    REG_TC2_RC1 = eom_pulse_length;
    REG_TC2_RA1 = eom_pulse_length;  
  }
}


void setup() 
{
  SerialUSB.begin(9600);

  pinMode(1, INPUT);  // Coupled to pin 5
  
  pmc_enable_periph_clk(ID_TC6);    
  pmc_enable_periph_clk(ID_TC7); 
  TC_EnablePin(TC2, 1, TIOA | TIOB);

    TC_Configure(TC2, 1,
    TC_CMR_TCCLKS_TIMER_CLOCK1 |
    TC_CMR_WAVE |
    TC_CMR_WAVSEL_UP_RC |
    TC_CMR_EEVTEDG_RISING |
    TC_CMR_EEVT_TIOB |
    TC_CMR_ACPA_CLEAR |
    TC_CMR_AEEVT_SET |
    TC_CMR_ACPC_SET |
    TC_CMR_ENETRG
  );
  TC_Start(TC2, 1);    
  TC_EnableInterrupt(TC2, 1, TC_IER_CPAS); // Enable interrupt on RA loading

  setTimerRegisters();

  // Measure EOM pulse width
  TC_Configure(TC2, 0,
    TC_CMR_TCCLKS_TIMER_CLOCK1 |
    TC_CMR_ETRGEDG_RISING | 
    TC_CMR_ABETRG |
    TC_CMR_LDRA_FALLING
  );

  TC_EnablePin(TC2, 0, TIOA);
  TC_EnableInterrupt(TC2, 0, TC_IER_LDRAS); // Enable interrupt on RA loading
  TC_Start(TC2, 0);  
}

void loop() {
  if (SerialUSB.available() > 0) 
  {
   
   if (SerialUSB.available()) 
   {
      // get command
      char command = (char) SerialUSB.read();
      
      // get parameter
      SerialUSB.readBytes(param, 4);
      switch (command)
      {
         // Admin commands
         //================================================
         case MSG_IDENTIFY:
         {            
            SendMessage(MSG_IDENTITY, "PLIM Laser Modulator", 20);
         } break;
         
         // Setup commands
         //================================================
         case MSG_SET_MODULATION:
         {
            active_modulation = *param_ui;
            setTimerRegisters();
         } break;
         //------------------------------------------------
         case MSG_SET_NUM_PIXELS:
         {
            n_px = *param_ui;
         } break;

      }

   }
  }

}
