///////////////////////////////////////////////////////////////////////////////
// LameXP - Audio Encoder Front-End
// Copyright (C) 2004-2016 LoRd_MuldeR <MuldeR2@GMX.de>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version, but always including the *additional*
// restrictions defined in the "License.txt" file.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// http://www.gnu.org/licenses/gpl-2.0.txt
///////////////////////////////////////////////////////////////////////////////

#include "Decoder_ADPCM.h"

//Internal
#include "Global.h"

//MUtils
#include <MUtils/Exception.h>

//Qt
#include <QDir>
#include <QProcess>
#include <QRegExp>

ADPCMDecoder::ADPCMDecoder(void)
:
	m_binary(lamexp_tools_lookup("sox.exe"))
{
	if(m_binary.isEmpty())
	{
		MUTILS_THROW("Error initializing Vorbis decoder. Tool 'sox.exe' is not registred!");
	}
}

ADPCMDecoder::~ADPCMDecoder(void)
{
}

bool ADPCMDecoder::decode(const QString &sourceFile, const QString &outputFile, volatile bool *abortFlag)
{
	QProcess process;
	QStringList args;

	args << "-V3" << "-S" << "--temp" << ".";
	args << QDir::toNativeSeparators(sourceFile);
	args << "-e" << "signed-integer";
	args << QDir::toNativeSeparators(outputFile);

	if(!startProcess(process, m_binary, args, QFileInfo(outputFile).canonicalPath()))
	{
		return false;
	}

	bool bTimeout = false;
	bool bAborted = false;

	QRegExp regExp("In:(\\d+)(\\.\\d+)*%");

	while(process.state() != QProcess::NotRunning)
	{
		if(*abortFlag)
		{
			process.kill();
			bAborted = true;
			emit messageLogged("\nABORTED BY USER !!!");
			break;
		}
		process.waitForReadyRead(m_processTimeoutInterval);
		if(!process.bytesAvailable() && process.state() == QProcess::Running)
		{
			process.kill();
			qWarning("Sox process timed out <-- killing!");
			emit messageLogged("\nPROCESS TIMEOUT !!!");
			bTimeout = true;
			break;
		}
		while(process.bytesAvailable() > 0)
		{
			QByteArray line = process.readLine();
			QString text = QString::fromUtf8(line.constData()).simplified();
			if(regExp.lastIndexIn(text) >= 0)
			{
				bool ok = false;
				int progress = regExp.cap(1).toInt(&ok);
				if(ok) emit statusUpdated(progress);
			}
			else if(!text.isEmpty())
			{
				emit messageLogged(text);
			}
		}
	}

	process.waitForFinished();
	if(process.state() != QProcess::NotRunning)
	{
		process.kill();
		process.waitForFinished(-1);
	}
	
	emit statusUpdated(100);
	emit messageLogged(QString().sprintf("\nExited with code: 0x%04X", process.exitCode()));

	if(bTimeout || bAborted || process.exitCode() != EXIT_SUCCESS || QFileInfo(outputFile).size() == 0)
	{
		return false;
	}
	
	return true;
}

bool ADPCMDecoder::isFormatSupported(const QString &containerType, const QString &containerProfile, const QString &formatType, const QString &formatProfile, const QString &formatVersion)
{
	if(containerType.compare("Wave", Qt::CaseInsensitive) == 0)
	{
		if(formatType.compare("ADPCM", Qt::CaseInsensitive) == 0)
		{
			return true;
		}
	}

	if(containerType.compare("AIFF", Qt::CaseInsensitive) == 0 || containerType.compare("AU", Qt::CaseInsensitive) == 0)
	{
		if(formatType.compare("PCM", Qt::CaseInsensitive) == 0 || formatType.compare("ADPCM", Qt::CaseInsensitive) == 0)
		{
			return true;
		}
	}

	return false;
}

const AbstractDecoder::supportedType_t *ADPCMDecoder::supportedTypes(void)
{
	static const char *exts[][3] =
	{
		{ "wav",         NULL },
		{ "aif", "aiff", NULL },
		{ "au" , "snd",  NULL }
	};

	static const supportedType_t s_supportedTypes[] =
	{
		{ "Microsoft ADPCM", exts[0] },
		{ "Apple/SGI AIFF",  exts[1] },
		{ "Sun/NeXT Au",     exts[2] },
		{ NULL, NULL }
	};

	return s_supportedTypes;
}
