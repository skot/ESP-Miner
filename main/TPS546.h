#ifndef TPS546_H_
#define TPS546_H_

#define TPS546_I2CADDR         0x24  //< TPS546 i2c address
#define TPS546_MANUFACTURER_ID 0xFE  //< Manufacturer ID
#define TPS546_REVISION        0xFF  //< Chip revision

struct tps546_settings_t
{
  int switch_frequency;

  /* vin voltage */
  int vin_on;	/* voltage level to start power conversion */
  int vin_off;  /* voltage level to stop power conversion */
  int vin_uv_warn_limit;
  int vin_ov_fault_limit;
  int vin_ov_fault_response;

  /* vout voltage */
  int vout_transition_rate;
  int vout_scale_loop;
  int vout_trim;
  int vout_max;
  int vout_ov_fault_limit;
  int vout_ov_warn_limit;
  int vout_margin_high;
  int vout_command;
  int vout_margin_low;
  int vout_uv_warn_limit;
  int vout_uv_fault_limit;
  int vout_min;

  /* iout current */
  int iout_oc_warn_limit;
  int iout_oc_fault_limit;
  int iout_oc_fault_response;
  int iout_cal_gain;
  int iout_cal_offset;

  /* temperature */
  int ot_warn_limit;
  int ot_fault_limit;
  int ot_fault_response;

  /* timing */
  int ton_delay;
  int ton_rise;
  int ton_max_fault_limit;
  int ton_max_fault_response;
  int toff_delay;
  int toff_fall;

};

void TPS546_init(void);
int TPS546_get_temperature(void);
void TPS546_set_vout(int millivolts);
void TPS546_show_voltage_settings(void);

#endif /* TPS546_H_ */
