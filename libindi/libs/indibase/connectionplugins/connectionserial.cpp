/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "indicom.h"
#include "indilogger.h"
#include "connectionserial.h"

namespace Connection
{

extern const char *CONNECTION_TAB;

Serial::Serial(INDI::DefaultDevice *dev) : Interface(dev)
{
#ifdef __APPLE__
    IUFillText(&PortT[0],"PORT","Port","/dev/cu.usbserial");
#else
    IUFillText(&PortT[0],"PORT","Port","/dev/ttyUSB0");
#endif
    IUFillTextVector(&PortTP,PortT,1, dev->getDeviceName(),"DEVICE_PORT","Ports",CONNECTION_TAB,IP_RW,60,IPS_IDLE);

    IUFillSwitch(&AutoSearchS[0], "ENABLED", "Enabled", ISS_ON);
    IUFillSwitch(&AutoSearchS[1], "DISABLED", "Disabled", ISS_OFF);
    IUFillSwitchVector(&AutoSearchSP, AutoSearchS, 2, dev->getDeviceName(),"DEVICE_AUTO_SEARCH", "Auto Search", CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&RefreshS[0], "Scan Ports", "Scan Ports", ISS_ON);
    IUFillSwitchVector(&RefreshSP, RefreshS, 1, dev->getDeviceName(),"DEVICE_PORT_SCAN", "Refresh", CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillSwitch(&BaudRateS[0], "9600", "", ISS_ON);
    IUFillSwitch(&BaudRateS[1], "19200", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[2], "38400", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[3], "57600", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[4], "115200", "", ISS_OFF);
    IUFillSwitch(&BaudRateS[5], "230400", "", ISS_OFF);
    IUFillSwitchVector(&BaudRateSP, BaudRateS, 6, dev->getDeviceName(),"DEVICE_BAUD_RATE", "Baud Rate", CONNECTION_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
}

Serial::~Serial()
{

}

bool Serial::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(!strcmp(dev,device->getDeviceName()))
    {
        // Serial Port
        if(!strcmp(name,PortTP.name))
        {
            IUUpdateText(&PortTP,texts,names,n);
            PortTP.s=IPS_OK;
            IDSetText(&PortTP,NULL);
            return true;
        }
    }

    return false;
}

bool Serial::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(!strcmp(dev,device->getDeviceName()))
    {
        if (!strcmp(name, BaudRateSP.name))
        {
            IUUpdateSwitch(&BaudRateSP, states, names, n);
            BaudRateSP.s = IPS_OK;
            IDSetSwitch(&BaudRateSP, NULL);
            return true;
        }

        if (!strcmp(name, AutoSearchSP.name))
        {
            IUUpdateSwitch(&AutoSearchSP, states, names, n);
            AutoSearchSP.s = IPS_OK;
            IDSetSwitch(&AutoSearchSP, NULL);

            return true;
        }

        if (!strcmp(name, RefreshSP.name))
        {
            IUUpdateSwitch(&RefreshSP, states, names, n);

            RefreshSP.s = refresh() ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&RefreshSP, NULL);

            return true;
        }

        if (!strcmp(name, SystemPortSP.name))
        {
            IUUpdateSwitch(&SystemPortSP, states, names, n);

            ISwitch *sp = IUFindOnSwitch(&SystemPortSP);
            if (sp)
            {
                IUSaveText(&PortT[0], sp->name);
                IDSetText(&PortTP, NULL);
            }

            SystemPortSP.s = IPS_OK;
            IDSetSwitch(&SystemPortSP, NULL);

            return true;
        }
    }

    return false;
}

bool Serial::Connect()
{
    uint32_t baud = atoi(IUFindOnSwitch(&BaudRateSP)->name);
    bool rc = Connect(PortT[0].text, baud);

    if (rc)
        return processHandshake();

    if (AutoSearchS[0].s == ISS_ON)
    {
        DEBUGF(INDI::Logger::DBG_WARNING, "Connection to %s @ %d failed. Starting Auto Search...", PortT[0].text, baud);
        for (int i=0; i < SystemPortSP.nsp; i++)
        {
            DEBUGF(INDI::Logger::DBG_DEBUG, "Trying connection to %s @ %d ...", SystemPortS[i].name, baud);
            if (Connect(SystemPortS[i].name, baud))
            {
                IUSaveText(&PortT[0], SystemPortS[i].name);
                IDSetText(&PortTP, NULL);
                rc = processHandshake();
                if (rc)
                    return true;
            }
        }
    }

    return rc;
}

bool Serial::processHandshake()
{
    DEBUG(INDI::Logger::DBG_DEBUG, "Connection successful, attempting handshake...");
    bool rc = Handshake();
    if (rc)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "%s is online.", getDeviceName());
        device->saveConfig(true, "DEVICE_PORT");
        device->saveConfig(true, "DEVICE_BAUD_RATE");
    }
    else
        DEBUG(INDI::Logger::DBG_DEBUG, "Handshake failed.");

    return rc;
}

bool Serial::Connect(const char *port, uint32_t baud)
{
    if (device->isSimulation())
        return true;

    int connectrc=0;
    char errorMsg[MAXRBUF];

    DEBUGF(INDI::Logger::DBG_DEBUG, "Connecting to %s",port);

    if ( (connectrc = tty_connect(port, baud, 8, 0, 1, &PortFD)) != TTY_OK)
    {
        tty_error_msg(connectrc, errorMsg, MAXRBUF);

        DEBUGF(INDI::Logger::DBG_ERROR,"Failed to connect to port (%s). Error: %s", port, errorMsg);

        return false;
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "Port FD %d",PortFD);

    return true;
}

bool Serial::Disconnect()
{
    if (PortFD > 0)
    {
        tty_disconnect(PortFD);
        PortFD = -1;
    }

    return true;
}

void Serial::Activated()
{
    device->defineText(&PortTP);
    device->loadConfig(true, "DEVICE_PORT");

    device->defineSwitch(&BaudRateSP);
    device->loadConfig(true, "DEVICE_BAUD_RATE");

    device->defineSwitch(&AutoSearchSP);
    device->loadConfig(true, "DEVICE_AUTO_SEARCH");

    device->defineSwitch(&RefreshSP);
    refresh();
}

void Serial::Deactivated()
{
    device->deleteProperty(PortTP.name);
    device->deleteProperty(BaudRateSP.name);
    device->deleteProperty(AutoSearchSP.name);

    device->deleteProperty(RefreshSP.name);
    device->deleteProperty(SystemPortSP.name);
    delete [] SystemPortS;
    SystemPortS = NULL;
}

bool Serial::saveConfigItems(FILE *fp)
{
    IUSaveConfigText(fp, &PortTP);
    IUSaveConfigSwitch(fp, &BaudRateSP);
    IUSaveConfigSwitch(fp, &AutoSearchSP);

    return true;
}

void Serial::setDefaultPort(const char *defaultPort)
{
    IUSaveText(&PortT[0], defaultPort);
}

void Serial::setDefaultBaudIndex(int newIndex)
{
    if (newIndex < 0 || newIndex >= BaudRateSP.nsp)
        return;

    IUResetSwitch(&BaudRateSP);
    BaudRateS[newIndex].s = ISS_ON;
}

const uint32_t Serial::baud()
{
    return atoi(IUFindOnSwitch(&BaudRateSP)->name);
}

bool Serial::refresh()
{
    if (SystemPortS)
        device->deleteProperty(SystemPortSP.name);

    delete [] SystemPortS;
    std::vector<std::string> m_Ports;

#if defined(__APPLE__)
        FILE *devs = popen("ls /dev/cu.*", "r");
#else
    // regular ls has many problems here, using find
    FILE *devs = popen("find /dev -name 'ttyUSB*' -o -name 'ttyACM*' -o -name 'rfcomm*'", "r");
#endif
        if (devs)
        {
            char line[64];
            while(fgets(line,64,devs)!=NULL && m_Ports.size() < 10)
            {
               std::string s(line);
               s.erase(s.find_last_not_of(" \n\r\t")+1);
               m_Ports.push_back(s);
            }
        }

        if (m_Ports.size() == 0)
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Failed to find any candidate ports on the system.");
            return false;
        }

        SystemPortS = new ISwitch[m_Ports.size()];
        ISwitch *sp = SystemPortS;
        for (int i=0; i < m_Ports.size(); i++)
        {
            IUFillSwitch(sp++, m_Ports[i].c_str(), m_Ports[i].c_str(), ISS_OFF);
        }

        IUFillSwitchVector(&SystemPortSP, SystemPortS, m_Ports.size(), device->getDeviceName(), "SYSTEM_PORTS", "System Ports", CONNECTION_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

        device->defineSwitch(&SystemPortSP);

        return true;
}

}