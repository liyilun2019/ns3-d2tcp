/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 Magister Solutions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Lauri Sormunen <lauri.sormunen@magister.fi>
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThreeGppHttpExample");

void
ServerConnectionEstablished (Ptr<const ThreeGppHttpServer>, Ptr<Socket>)
{
  NS_LOG_INFO ("Client has established a connection to the server.");
}

void
MainObjectGenerated (uint32_t size)
{
  NS_LOG_INFO ("Server generated a main object of " << size << " bytes.");
}

void
EmbeddedObjectGenerated (uint32_t size)
{
  NS_LOG_INFO ("Server generated an embedded object of " << size << " bytes.");
}

void
ServerTx (Ptr<const Packet> packet)
{
  NS_LOG_INFO ("Server sent a packet of " << packet->GetSize () << " bytes.");
}

void
ClientRx (Ptr<const Packet> packet, const Address &address)
{
  NS_LOG_INFO ("Client received a packet of " << packet->GetSize () << " bytes from " << address);
}

void
ClientMainObjectReceived (Ptr<const ThreeGppHttpClient>, Ptr<const Packet> packet)
{
  Ptr<Packet> p = packet->Copy ();
  ThreeGppHttpHeader header;
  p->RemoveHeader (header);
  if (header.GetContentLength () == p->GetSize ()
      && header.GetContentType () == ThreeGppHttpHeader::MAIN_OBJECT)
    {
      NS_LOG_INFO ("Client has successfully received a main object of "
                   << p->GetSize () << " bytes.");
    }
  else
    {
      NS_LOG_INFO ("Client failed to parse a main object. ");
    }
}

void
ClientEmbeddedObjectReceived (Ptr<const ThreeGppHttpClient>, Ptr<const Packet> packet)
{
  Ptr<Packet> p = packet->Copy ();
  ThreeGppHttpHeader header;
  p->RemoveHeader (header);
  if (header.GetContentLength () == p->GetSize ()
      && header.GetContentType () == ThreeGppHttpHeader::EMBEDDED_OBJECT)
    {
      NS_LOG_INFO ("Client has successfully received an embedded object of "
                   << p->GetSize () << " bytes.");
    }
  else
    {
      NS_LOG_INFO ("Client failed to parse an embedded object. ");
    }
}

// Default Network Topology
//
//       10.1.1.0
// n0 -------------- n1   n2   n3   n4
//    point-to-point  |    |    |    |
//                    ================
//                      LAN 10.1.2.0


void cli_trace(Ptr<ThreeGppHttpClient> httpClient){
  // Example of connecting to the trace sources
  httpClient->TraceConnectWithoutContext ("RxMainObject", MakeCallback (&ClientMainObjectReceived));
  httpClient->TraceConnectWithoutContext ("RxEmbeddedObject", MakeCallback (&ClientEmbeddedObjectReceived));
  httpClient->TraceConnectWithoutContext ("Rx", MakeCallback (&ClientRx));
}

int
main (int argc, char *argv[])
{
  double simTimeSec = 10;
  CommandLine cmd (__FILE__);
  cmd.AddValue ("SimulationTime", "Length of simulation in seconds.", simTimeSec);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnableAll (LOG_PREFIX_TIME);
  //LogComponentEnableAll (LOG_PREFIX_FUNC);
  LogComponentEnable ("ThreeGppHttpClient", LOG_INFO);
  LogComponentEnable ("ThreeGppHttpServer", LOG_INFO);
  LogComponentEnable ("ThreeGppHttpExample", LOG_INFO);
  LogComponentEnable ("TcpD2tcp",LOG_INFO);

  std::string tcpTypeId = "TcpD2tcp";
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::" + tcpTypeId));

  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1448));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (2));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));

  // Set default parameters for RED queue disc
  Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (true));
  // ARED may be used but the queueing delays will increase; it is disabled
  // here because the SIGCOMM paper did not mention it
  // Config::SetDefault ("ns3::RedQueueDisc::ARED", BooleanValue (true));
  // Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (true));
  Config::SetDefault ("ns3::RedQueueDisc::UseHardDrop", BooleanValue (false));
  Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (1500));
  // Triumph and Scorpion switches used in DCTCP Paper have 4 MB of buffer
  // If every packet is 1500 bytes, 2666 packets can be stored in 4 MB
  Config::SetDefault ("ns3::RedQueueDisc::MaxSize", QueueSizeValue (QueueSize ("2666p")));
  // DCTCP tracks instantaneous queue length only; so set QW = 1
  Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (1));
  Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (20));
  Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (60));

  uint32_t nCsma = 3;
  nCsma = nCsma == 0 ? 1 : nCsma;

  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  NodeContainer csmaNodes;
  csmaNodes.Add (p2pNodes.Get (1));
  csmaNodes.Create (nCsma);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("10Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (656000)));

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  InternetStackHelper stack;
  stack.Install (p2pNodes.Get (0));
  stack.Install (csmaNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);

  NS_LOG_INFO ("Create applications.");
  Ipv4Address serverAddress = csmaInterfaces.GetAddress (3);

  // Create HTTP server helper
  ThreeGppHttpServerHelper serverHelper (serverAddress);

  // Install HTTP server
  ApplicationContainer serverApps = serverHelper.Install (csmaNodes.Get (3));
  Ptr<ThreeGppHttpServer> httpServer = serverApps.Get (0)->GetObject<ThreeGppHttpServer> ();

  // Example of connecting to the trace sources
  httpServer->TraceConnectWithoutContext ("ConnectionEstablished",
                                          MakeCallback (&ServerConnectionEstablished));
  httpServer->TraceConnectWithoutContext ("MainObject", MakeCallback (&MainObjectGenerated));
  httpServer->TraceConnectWithoutContext ("EmbeddedObject", MakeCallback (&EmbeddedObjectGenerated));
  httpServer->TraceConnectWithoutContext ("Tx", MakeCallback (&ServerTx));

  // Setup HTTP variables for the server
  PointerValue varPtr;
  httpServer->GetAttribute ("Variables", varPtr);
  Ptr<ThreeGppHttpVariables> httpVariables = varPtr.Get<ThreeGppHttpVariables> ();
  httpVariables->SetMainObjectSizeMean (102400); // 100kB
  httpVariables->SetMainObjectSizeStdDev (40960); // 40kB
  httpVariables->SetMainObjectGenerationDelay(Seconds(1.0));
  httpVariables->SetEmbeddedObjectGenerationDelay(Seconds(0.5));

  // Create HTTP client helper
  ThreeGppHttpClientHelper clientHelper (serverAddress);

  // Install HTTP client
  NodeContainer cli_nodes = NodeContainer(csmaNodes.Get(0),csmaNodes.Get(1));
  ApplicationContainer clientApps = clientHelper.Install (cli_nodes);
  Ptr<ThreeGppHttpClient> httpClient0 = clientApps.Get (0)->GetObject<ThreeGppHttpClient> ();
  // cli_trace(httpClient0);
  Ptr<ThreeGppHttpClient> httpClient1 = clientApps.Get (1)->GetObject<ThreeGppHttpClient> ();
  // cli_trace(httpClient1);
  // Ptr<ThreeGppHttpClient> httpClient2 = clientApps.Get (2)->GetObject<ThreeGppHttpClient> ();
  // cli_trace(httpClient2);
  clientApps.Stop (Seconds (simTimeSec));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
