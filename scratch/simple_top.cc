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
#include "ns3/traffic-control-module.h"

using namespace ns3;
int not_miss_count=0;
int all_cnt=0;

NS_LOG_COMPONENT_DEFINE ("ThreeGppHttpExample");

void
ServerConnectionEstablished (Ptr<const ThreeGppHttpServer>, Ptr<Socket>)
{
  // NS_LOG_INFO ("Client has established a connection to the server.");
}

void
MainObjectGenerated (uint32_t size)
{
  // NS_LOG_INFO ("Server generated a main object of " << size << " bytes.");
}

void
EmbeddedObjectGenerated (uint32_t size)
{
  // NS_LOG_INFO ("Server generated an embedded object of " << size << " bytes.");
}

void
ServerTx (Ptr<const Packet> packet)
{
  // NS_LOG_INFO ("Server sent a packet of " << packet->GetSize () << " bytes.");
}

void
ClientRx (Ptr<const Packet> packet, const Address &address)
{
  // NS_LOG_INFO ("Client received a packet of " << packet->GetSize () << " bytes from " << InetSocketAddress::ConvertFrom(address).GetIpv4 ());
}

void
ClientMainObjectReceived (Ptr<const ThreeGppHttpClient> client, Ptr<const Packet> packet)
{
  Ptr<Packet> p = packet->Copy ();
  ThreeGppHttpHeader header;
  p->RemoveHeader (header);
  if (header.GetContentLength () == p->GetSize ()
      && header.GetContentType () == ThreeGppHttpHeader::MAIN_OBJECT)
    {
      NS_LOG_INFO ("Client has successfully received a main object of "
                   << p->GetSize () << " bytes."<<" deadline is :"<<client->GetDeadline());
      if(Simulator::Now()<=client->GetDeadline()){
        not_miss_count++;
      }
      all_cnt++;
      NS_LOG_INFO("not_miss_count = "<<not_miss_count<<", all_cnt = "<<all_cnt);
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
// T ------ S(10)
//   


int
main (int argc, char *argv[])
{
  double simTimeSec = 20;
  std::size_t node_cnt=16;
  std::size_t next_cnt=4;
  Time generationDelay = Seconds(0.1);
  std::size_t package_size = 150*1024*1042;
  Time delay = Seconds(1);
  CommandLine cmd (__FILE__);
  cmd.AddValue ("SimulationTime", "Length of simulation in seconds.", simTimeSec);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnableAll (LOG_PREFIX_TIME);
  //LogComponentEnableAll (LOG_PREFIX_FUNC);
  // LogComponentEnable ("ThreeGppHttpClient", LOG_INFO);
  // LogComponentEnable ("ThreeGppHttpServer", LOG_INFO);
  LogComponentEnable ("ThreeGppHttpExample", LOG_INFO);
  // LogComponentEnable ("TcpD2tcp",LOG_INFO);

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


  NodeContainer S;
  Ptr<Node> T = CreateObject<Node> ();
  S.Create (node_cnt);

  PointToPointHelper pointToPointSR;
  pointToPointSR.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  pointToPointSR.SetChannelAttribute ("Delay", StringValue ("10us"));

  // 建立拓扑结构
  std::vector<NetDeviceContainer> ST;
  ST.reserve (node_cnt);

  for (std::size_t i = 0; i < node_cnt; i++)
    {
      Ptr<Node> n = S.Get (i);
      ST.push_back (pointToPointSR.Install (n, T));
    }

  // 建立协议栈，使用red queue
  InternetStackHelper stack;
  stack.InstallAll ();

  TrafficControlHelper tchRed10;
  // MinTh = 50, MaxTh = 150 recommended in ACM SIGCOMM 2010 DCTCP Paper
  // This yields a target (MinTh) queue depth of 60us at 10 Gb/s
  tchRed10.SetRootQueueDisc ("ns3::RedQueueDisc",
                             "LinkBandwidth", StringValue ("10Gbps"),
                             "LinkDelay", StringValue ("10us"),
                             "MinTh", DoubleValue (50),
                             "MaxTh", DoubleValue (150));

  TrafficControlHelper tchRed1;
  // MinTh = 20, MaxTh = 60 recommended in ACM SIGCOMM 2010 DCTCP Paper
  // This yields a target queue depth of 250us at 1 Gb/s
  tchRed1.SetRootQueueDisc ("ns3::RedQueueDisc",
                            "LinkBandwidth", StringValue ("1Gbps"),
                            "LinkDelay", StringValue ("10us"),
                            "MinTh", DoubleValue (20),
                            "MaxTh", DoubleValue (60));
  for (std::size_t i = 0; i < node_cnt; i++)
    {
      tchRed1.Install (ST[i].Get (1));
    }

  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> ipST;
  ipST.reserve (node_cnt);
  address.SetBase ("10.1.1.0", "255.255.255.0");
  for (std::size_t i = 0; i < node_cnt; i++)
    {
      ipST.push_back (address.Assign (ST[i]));
      address.NewNetwork ();
    }
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // 对每个节点建立server
  for (std::size_t i = 0; i<node_cnt ;i++){
    Ipv4Address serverAddress = ipST[i].GetAddress (0);
    NS_LOG_INFO("Create server " << serverAddress);

    // Create HTTP server helper
    ThreeGppHttpServerHelper serverHelper (serverAddress);

    // Install HTTP server
    ApplicationContainer serverApps = serverHelper.Install (S.Get(i));

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
    httpVariables->SetMainObjectSizeMean (package_size); 
    httpVariables->SetMainObjectSizeStdDev (10240);
    httpVariables->SetMainObjectGenerationDelay(generationDelay);
    // httpVariables->SetEmbeddedObjectGenerationDelay(Seconds(0.5));
  }

  // 对每个节点，建立next_cnt个clinet，向后next_cnt个server发请求
  for (std::size_t i = 0; i<node_cnt ;i++){
    for (std::size_t j=0 ; j < next_cnt; j++){
      std::size_t nxt = (i+j+1)%node_cnt;
      Ipv4Address serverAddress = ipST[nxt].GetAddress (0);
      Ipv4Address clinetAddress = ipST[i].GetAddress (0);
      NS_LOG_INFO("Create clinet " << clinetAddress << " to server"<< serverAddress);
      ThreeGppHttpClientHelper clientHelper (serverAddress);
      ApplicationContainer clientApps = clientHelper.Install (S.Get(i));
      Ptr<ThreeGppHttpClient> httpClient = clientApps.Get (0)->GetObject<ThreeGppHttpClient> ();
      httpClient->SetDelay(delay+generationDelay);
      // Example of connecting to the trace sources
      httpClient->TraceConnectWithoutContext ("RxMainObject", MakeCallback (&ClientMainObjectReceived));
      // httpClient->TraceConnectWithoutContext ("RxEmbeddedObject", MakeCallback (&ClientEmbeddedObjectReceived));
      httpClient->TraceConnectWithoutContext ("Rx", MakeCallback (&ClientRx));
      clientApps.Stop (Seconds (simTimeSec));
    }
  }

  NS_LOG_INFO("~~~~~~~~~~~~~~~~~ Running ~~~~~~~~~~~~~~~");
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
