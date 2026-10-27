package com.yuminaga.motonav;
public class SimulationSpeedPolicyTest {
 public static void main(String[] args){
  for(int n:new int[]{60,100,120})if(!SimulationSpeedPolicy.allowed(n)||SimulationSpeedPolicy.normalize(n)!=n)throw new AssertionError(n);
  for(int n:new int[]{-1,0,59,61,99,101,119,121,180,Integer.MAX_VALUE})if(SimulationSpeedPolicy.allowed(n)||SimulationSpeedPolicy.normalize(n)!=60)throw new AssertionError(n);
  System.out.println("Simulation speed whitelist/default passed");
 }
}
