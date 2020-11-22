/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 NITK Surathkal
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
 * Author: Shravya K.S. <shravya.ks0@gmail.com>
 *
 */

#include "tcp-d2tcp.h"
#include "ns3/log.h"
#include "math.h"
#include "ns3/tcp-socket-state.h"
#include "ns3/core-module.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpD2tcp");

NS_OBJECT_ENSURE_REGISTERED (TcpD2tcp);

TypeId TcpD2tcp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpD2tcp")
    .SetParent<TcpNewReno> ()
    .AddConstructor<TcpD2tcp> ()
    .SetGroupName ("Internet")
    .AddAttribute ("DctcpShiftG",
                   "Parameter G for updating dctcp_alpha",
                   DoubleValue (0.0625),
                   MakeDoubleAccessor (&TcpD2tcp::m_g),
                   MakeDoubleChecker<double> (0, 1))
    .AddAttribute ("DctcpAlphaOnInit",
                   "Initial alpha value",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&TcpD2tcp::SetDctcpAlpha),
                   MakeDoubleChecker<double> (0, 1))
    .AddAttribute ("UseEct0",
                   "Use ECT(0) for ECN codepoint, if false use ECT(1)",
                   BooleanValue (true),
                   MakeBooleanAccessor (&TcpD2tcp::m_useEct0),
                   MakeBooleanChecker ())
  ;
  return tid;
}

std::string TcpD2tcp::GetName () const
{
  return "TcpD2tcp";
}

TcpD2tcp::TcpD2tcp ()
  : TcpNewReno ()
{
  NS_LOG_FUNCTION (this);
  m_ackedBytesEcn = 0;
  m_ackedBytesTotal = 0;
  m_priorRcvNxt = SequenceNumber32 (0);
  m_priorRcvNxtFlag = false;
  m_nextSeq = SequenceNumber32 (0);
  m_nextSeqFlag = false;
  m_ceState = false;
  m_delayedAckReserved = false;
}

TcpD2tcp::TcpD2tcp (const TcpD2tcp& sock)
  : TcpNewReno (sock),
    m_ackedBytesEcn (sock.m_ackedBytesEcn),
    m_ackedBytesTotal (sock.m_ackedBytesTotal),
    m_priorRcvNxt (sock.m_priorRcvNxt),
    m_priorRcvNxtFlag (sock.m_priorRcvNxtFlag),
    m_alpha (sock.m_alpha),
    m_nextSeq (sock.m_nextSeq),
    m_nextSeqFlag (sock.m_nextSeqFlag),
    m_ceState (sock.m_ceState),
    m_delayedAckReserved (sock.m_delayedAckReserved),
    m_g (sock.m_g),
    m_useEct0 (sock.m_useEct0)
{
  NS_LOG_FUNCTION (this);
}

TcpD2tcp::~TcpD2tcp (void)
{
  NS_LOG_FUNCTION (this);
}

Ptr<TcpCongestionOps> TcpD2tcp::Fork (void)
{
  NS_LOG_FUNCTION (this);
  return CopyObject<TcpD2tcp> (this);
}

void
TcpD2tcp::Init (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);
  NS_LOG_INFO (this << " Enabling DctcpEcn for DCTCP");
  tcb->m_useEcn = TcpSocketState::On;
  tcb->m_ecnMode = TcpSocketState::DctcpEcn;
  tcb->m_ectCodePoint = m_useEct0 ? TcpSocketState::Ect0 : TcpSocketState::Ect1;
}

void
TcpD2tcp::ReduceCwnd (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);
  uint32_t val = static_cast<uint32_t> ((1 - m_alpha / 2.0) * tcb->m_cWnd);
  tcb->m_cWnd = std::max (val, tcb->m_segmentSize);
}

void
TcpD2tcp::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time &rtt)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt);
  m_ackedBytesTotal += segmentsAcked * tcb->m_segmentSize;
  if (tcb->m_ecnState == TcpSocketState::ECN_ECE_RCVD)
    {
      m_ackedBytesEcn += segmentsAcked * tcb->m_segmentSize;
    }
  if (m_nextSeqFlag == false)
    {
      m_nextSeq = tcb->m_nextTxSequence;
      m_nextSeqFlag = true;
    }
  if (tcb->m_lastAckedSeq >= m_nextSeq)
    {
      double bytesEcn = 0.0;
      if (m_ackedBytesTotal >  0)
        {
          bytesEcn = static_cast<double> (m_ackedBytesEcn * 1.0 / m_ackedBytesTotal);
        }
      m_alpha = (1.0 - m_g) * m_alpha + m_g * bytesEcn;
      uint32_t txTotal = tcb->m_TxTotal;
      Time deadline = tcb->m_deadline;
      Time remain = deadline - Simulator::Now ();
      Time rtt = tcb->m_lastRtt;
      if ((remain > Seconds(0.0)) && (txTotal > 0)) {
        int64x64_t r = remain/rtt;
        double d  =  r.GetDouble();
        double tc = 4.0 * (txTotal - m_ackedBytesTotal) / (3.0 * (tcb->m_cWnd));
        double p  = tc/d;
        m_alpha = pow(m_alpha,p);
      }
      NS_LOG_INFO (this << " bytesEcn " << bytesEcn << ", m_alpha " << m_alpha
        <<", remain time "<<remain <<" txTotal "<<txTotal <<" deadline is : "<<deadline<<" now is : "<<Simulator::Now());
      Reset (tcb);
    }
}

void
TcpD2tcp::SetDctcpAlpha (double alpha)
{
  NS_LOG_FUNCTION (this << alpha);
  m_alpha = alpha;
}

void
TcpD2tcp::Reset (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);
  m_nextSeq = tcb->m_nextTxSequence;
  m_ackedBytesEcn = 0;
  m_ackedBytesTotal = 0;
}

void
TcpD2tcp::CeState0to1 (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);
  if (!m_ceState && m_delayedAckReserved && m_priorRcvNxtFlag)
    {
      SequenceNumber32 tmpRcvNxt;
      /* Save current NextRxSequence. */
      tmpRcvNxt = tcb->m_rxBuffer->NextRxSequence ();

      /* Generate previous ACK without ECE */
      tcb->m_rxBuffer->SetNextRxSequence (m_priorRcvNxt);
      tcb->m_sendEmptyPacketCallback (TcpHeader::ACK);

      /* Recover current RcvNxt. */
      tcb->m_rxBuffer->SetNextRxSequence (tmpRcvNxt);
    }

  if (m_priorRcvNxtFlag == false)
    {
      m_priorRcvNxtFlag = true;
    }
  m_priorRcvNxt = tcb->m_rxBuffer->NextRxSequence ();
  m_ceState = true;
  tcb->m_ecnState = TcpSocketState::ECN_CE_RCVD;
}

void
TcpD2tcp::CeState1to0 (Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);
  if (m_ceState && m_delayedAckReserved && m_priorRcvNxtFlag)
    {
      SequenceNumber32 tmpRcvNxt;
      /* Save current NextRxSequence. */
      tmpRcvNxt = tcb->m_rxBuffer->NextRxSequence ();

      /* Generate previous ACK with ECE */
      tcb->m_rxBuffer->SetNextRxSequence (m_priorRcvNxt);
      tcb->m_sendEmptyPacketCallback (TcpHeader::ACK | TcpHeader::ECE);

      /* Recover current RcvNxt. */
      tcb->m_rxBuffer->SetNextRxSequence (tmpRcvNxt);
    }

  if (m_priorRcvNxtFlag == false)
    {
      m_priorRcvNxtFlag = true;
    }
  m_priorRcvNxt = tcb->m_rxBuffer->NextRxSequence ();
  m_ceState = false;

  if (tcb->m_ecnState == TcpSocketState::ECN_CE_RCVD || tcb->m_ecnState == TcpSocketState::ECN_SENDING_ECE)
    {
      tcb->m_ecnState = TcpSocketState::ECN_IDLE;
    }
}

void
TcpD2tcp::UpdateAckReserved (Ptr<TcpSocketState> tcb,
                             const TcpSocketState::TcpCAEvent_t event)
{
  NS_LOG_FUNCTION (this << tcb << event);
  switch (event)
    {
    case TcpSocketState::CA_EVENT_DELAYED_ACK:
      if (!m_delayedAckReserved)
        {
          m_delayedAckReserved = true;
        }
      break;
    case TcpSocketState::CA_EVENT_NON_DELAYED_ACK:
      if (m_delayedAckReserved)
        {
          m_delayedAckReserved = false;
        }
      break;
    default:
      /* Don't care for the rest. */
      break;
    }
}

void
TcpD2tcp::CwndEvent (Ptr<TcpSocketState> tcb,
                     const TcpSocketState::TcpCAEvent_t event)
{
  NS_LOG_FUNCTION (this << tcb << event);
  switch (event)
    {
    case TcpSocketState::CA_EVENT_ECN_IS_CE:
      CeState0to1 (tcb);
      break;
    case TcpSocketState::CA_EVENT_ECN_NO_CE:
      CeState1to0 (tcb);
      break;
    case TcpSocketState::CA_EVENT_DELAYED_ACK:
    case TcpSocketState::CA_EVENT_NON_DELAYED_ACK:
      UpdateAckReserved (tcb, event);
      break;
    default:
      /* Don't care for the rest. */
      break;
    }
}

} // namespace ns3
