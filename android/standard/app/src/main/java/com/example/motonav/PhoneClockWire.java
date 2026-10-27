package com.example.motonav;
/** UTC epoch seconds and explicit phone offset, independent of language/locale. */
public final class PhoneClockWire {
 private PhoneClockWire(){}
 public static String encode(long wallMillis,int offsetSeconds){
  long seconds=wallMillis/1000;
  if(wallMillis<1577836800000L||seconds>=4102444800L||offsetSeconds < -50400||offsetSeconds>50400)return null;
  return "#TIME,"+seconds+","+offsetSeconds;
 }
}
