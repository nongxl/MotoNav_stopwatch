package com.yuminaga.motonav;
public class PhoneClockWireTest {
 public static void main(String[] args){
  if(!"#TIME,1793006580,28800".equals(PhoneClockWire.encode(1793006580123L,28800)))throw new AssertionError();
  if(!"#TIME,1793006580,-18000".equals(PhoneClockWire.encode(1793006580123L,-18000)))throw new AssertionError();
  if(PhoneClockWire.encode(0L,0)!=null||PhoneClockWire.encode(4102444800000L,0)!=null||PhoneClockWire.encode(1793006580123L,50401)!=null)throw new AssertionError();
  System.out.println("Phone time wire formatting/range checks passed");
 }
}
