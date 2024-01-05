﻿using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Ports;
using System.Threading;
using System.Windows.Forms;

namespace AT28CProgrammer
{
    public partial class Form1 : Form
    {

        #region Variabili (accesso privato)

        private SerialPort _serialPort;

        #endregion

        #region Costruttore

        public Form1()
        {
            InitializeComponent();

            // Inizializza porta seriale
            _serialPort = new SerialPort();
            _serialPort.BaudRate = 115200;
            _serialPort.DataBits = 8;
            _serialPort.Parity = Parity.None;
            _serialPort.StopBits = StopBits.One;
            _serialPort.PortName = "COM5";
            _serialPort.DtrEnable = false;

            // Enumerazione porte seriali disponibili
            string[] ports = SerialPort.GetPortNames();
            foreach (string port in ports)
            {
                cBSerialPorts.Items.Add(port);
            }
            if (cBSerialPorts.Items.Count > 0)
            {
                cBSerialPorts.SelectedIndex = 0;
                bConnetti.Enabled = true;
            }

        }

        #endregion

        #region Event Handlers Form1

        private void cBSerialPorts_SelectedIndexChanged(object sender, System.EventArgs e)
        {
            _serialPort.PortName = cBSerialPorts.SelectedItem.ToString();
            bConnetti.Enabled = true;
        }

        private void bConnetti_Click(object sender, System.EventArgs e)
        {
            if (Connetti())
            {
                Thread.Sleep(1000);

                String firmw = LeggiFirmware();
                if (firmw != "")
                {
                    tBInfo.Text = "";
                    tBInfo.AppendText("Firmware version " + firmw + "\r\n");
                    tBInfo.AppendText("---------------------------------------------\r\n");
                    tBInfo.AppendText("\r\n");

                    bDisconnetti.Enabled = true;
                    bConnetti.Enabled = false;
                    cBSerialPorts.Enabled = false;
                    //bInfo.Enabled = true;
                    //bDump.Enabled = true;
                    //bRAMDump.Enabled = true;
                    //bRAMWrite.Enabled = true;
                    toolStripStatusLabel1.Text = "Dispositivo connesso";
                }
                else
                {
                    Disconnetti();
                }
            }
        }

        private void bDisconnetti_Click(object sender, EventArgs e)
        {
            if (Disconnetti())
            {
                bDisconnetti.Enabled = false;
                bConnetti.Enabled = true;
                cBSerialPorts.Enabled = true;
                //bInfo.Enabled = false;
                //bDump.Enabled = false;
                //bRAMDump.Enabled = false;
                //bRAMWrite.Enabled = false;
                toolStripStatusLabel1.Text = "Dispositivo non connesso";
            }
        }

        private void bRead_Click(object sender, EventArgs e)
        {
            int size = 0;

            Cursor.Current = Cursors.WaitCursor;

            switch (cBTipoEEPROM.Text)
            {
                case "AT28C64":
                    size = 8192;
                    break;
                case "AT28C256":
                    size = 32768;
                    break;
            }

            if (size > 0)
            {
                progressBar1.Maximum = size;
                progressBar1.Value = 0;
                List<byte> datas = new List<byte>();

                saveFileDialog1.FileName = "dump.bin";
                if (saveFileDialog1.ShowDialog() == DialogResult.OK)
                {
                    tBInfo.AppendText("Inizio lettura EEPROM \r\n");
                    tBInfo.AppendText(DateTime.Now.ToString("HH:mm:ss") + "\r\n");

                    _serialPort.DiscardInBuffer();
                    _serialPort.DiscardOutBuffer();

                    ReadEEPROM(size, datas);

                    using (BinaryWriter writer = new BinaryWriter(File.Open(saveFileDialog1.FileName, FileMode.Create)))
                    {
                        byte[] buffer = datas.ToArray();
                        writer.Write(buffer, 0, buffer.Length);
                    }

                    tBInfo.AppendText("Lettura EEPROM terminato\r\n");
                    tBInfo.AppendText(DateTime.Now.ToString("HH:mm:ss") + "\r\n");
                    tBInfo.AppendText("\r\n");
                }
            }

            Cursor.Current = Cursors.Default;
        }

        private void bWrite_Click(object sender, EventArgs e)
        {
            int size = 0;

            Cursor.Current = Cursors.WaitCursor;

            /*
            switch (cBTipoEEPROM.Text)
            {
                case "AT28C64":
                    size = 8192;
                    break;
                case "AT28C256":
                    size = 32768;
                    break;
            }
            */

            //if (size > 0)
            {
                openFileDialog1.FileName = "dump.bin";
                if (openFileDialog1.ShowDialog() == DialogResult.OK)
                {
                    FileInfo fi = new FileInfo(openFileDialog1.FileName);
                    size = (int)fi.Length;

                    progressBar1.Maximum = size;
                    progressBar1.Value = 0;

                    tBInfo.AppendText("Inizio scrittura RAM \r\n");

                    _serialPort.DiscardInBuffer();
                    _serialPort.DiscardOutBuffer();

                    // Carica i dati
                    using (BinaryReader reader = new BinaryReader(File.Open(openFileDialog1.FileName, FileMode.Open)))
                    {
                        byte[] buffer = new byte[size];
                        reader.Read(buffer, 0, size);
                        WriteEEPROM(size, buffer);
                    }

                    tBInfo.AppendText("Scrittua RAM terminata\r\n");
                    tBInfo.AppendText("\r\n");
                }
            }

            Cursor.Current = Cursors.Default;
        }

        private void cBTipoEEPROM_SelectedIndexChanged(object sender, EventArgs e)
        {
            switch (cBTipoEEPROM.Text)
            {
                case "AT28C64":
                    groupBox1.Enabled = false;
                    break;
                case "AT28C256":
                    groupBox1.Enabled = true;
                    break;
            }
        }

        #endregion

        #region Metodi (Accesso privato)

        private bool Connetti()
        {
            try
            {
                _serialPort.Open();
                return _serialPort.IsOpen;
            }
            catch (Exception e)
            {
                return false;
            }
        }

        private bool Disconnetti()
        {
            _serialPort.Close();
            return !_serialPort.IsOpen;
        }

        private string LeggiFirmware()
        {
            String s = "";
            try
            {
                _serialPort.Write("VERSION=?\r");

                // 5 secondi di timeout
                int ExpiredTick = Environment.TickCount + 5000;
                while (Environment.TickCount < ExpiredTick)
                {
                    if (_serialPort.BytesToRead > 0)
                    {
                        s = _serialPort.ReadLine();
                        string[] split = s.Trim('\r').Split(new char[] { '=' });
                        if (split[0] == "+VERSION")
                        {
                            s = split[1];
                            break;
                        }
                    }
                }
                return s;
            }
            catch
            {
                return s;
            }
        }

        public void ReadEEPROM(int size, List<byte> datas)
        {
            string s = "";
            try
            {
                _serialPort.Write("READEEPROM=" + size.ToString() + "\r");

                byte[] buffer = new byte[size];
                int offset = 0;

                while (true) //(Environment.TickCount < ExpiredTick)
                {
                    int count = _serialPort.BytesToRead;
                    if (count > 0)
                    {
                        _serialPort.Read(buffer, offset, count);
                        offset += count;
                        progressBar1.Increment(count);
                    }

                    if (offset == size)
                    {
                        datas.AddRange(buffer);
                        break;
                    }
                }
            }
            catch { }
        }

        public void WriteEEPROM(int size, byte[] datas)
        {
            string s = "";
            try
            {
                _serialPort.Write("WRITEEEPROM=" + size.ToString() + "\r");
                // Invia 1 byte per volta
                for (int i = 0; i < datas.Length; i++)
                {
                    _serialPort.Write(datas, i, 1);
                    // Ritardo di 10 millisecondi per permettere la scrittura
                    Thread.Sleep(10);
                    progressBar1.Increment(1);
                }
            }
            catch { }
        }

        #endregion

    }
}
