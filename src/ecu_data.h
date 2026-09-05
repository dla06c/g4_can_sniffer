#pragma once

struct EcuData {
  float rpm = 0;
  float ect = 0;
  float iat = 0;
  float mgp = 0;
  float map = 0;
  float tps = 0;
  float gp_speed_1 = 0;  // Channel 2 DI 1 Freq - GP Speed 1, decoded as raw / 10

  float ignition_angle = 0;
  float injection_actual_pw = 0;
  float injection_effective_pw = 0;

  float lambda1 = 0;
  float lambda_target = 0;
  float lambda_error = 0;
  float lambda_status = 0;
  float lambda_temp = 0;

  float oil_temp = 0;
  float battery_v = 0;
  float fuel_pressure = 0;
  float oil_pressure = 0;

  float boost_target = 0;
  float boost_error = 0;
  float boost_p = 0;
  float boost_i = 0;
  float boost_d = 0;
  float boost_duty = 0;

  float trig1_err = 0;
  float internal_3v3 = 0;
  float internal_12v = 0;

  float aps_main = 0;
  float throttle_target = 0;
  float vvt_in_target = 0;
  float vvt_in_pos = 0;

  // Channel 2 gear value. UDP replay can still populate this as optional field 32.
  int gear = 0;

  unsigned long last_update_ms = 0;
};

extern EcuData ecu;
