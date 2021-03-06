/******************************************************************************

  This source file is part of the Avogadro project.

  Copyright (C) 2016 Kitware

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

#include "yaehmopout.h"

#include <QtCore/QFile>
#include <QtCore/QStringList>
#include <QtCore/QDebug>

YaehmopOut::YaehmopOut()
{
}

YaehmopOut::~YaehmopOut()
{
}

inline bool printAndReturnFalse(const QString& error)
{
  qDebug() << "Error in YaehmopOut:" << error;
  return false;
}

bool YaehmopOut::readBandData(const QString& data, QVector<band>& bands,
                              QVector<kpoint>& kpoints,
                              QVector<specialKPoint>& specialKPoints)
{
  bands.clear();
  kpoints.clear();
  specialKPoints.clear();

  QStringList lines = data.split(QRegExp("[\r\n]"), QString::SkipEmptyParts);

  while (!lines.isEmpty() && !lines[0].contains("#BAND_DATA"))
    lines.removeFirst();

  if (lines.isEmpty())
    return printAndReturnFalse("Band Data not found in readBandData()!");

  // These get printed from the status file and are not needed...
  foreach(const QString& line, lines) {
    if (line.contains("Error value from Diagonalization"))
      lines.removeOne(line);
  }

  // Right here, lines.size() must be at least 8
  if (lines.size() < 8)
    return printAndReturnFalse("Band data is too few lines!");

  size_t ind = 2;
  if (!lines[ind].contains("Special points"))
    return printAndReturnFalse("Special Points missing");
  size_t numSpecialPoints = lines[ind].split(" ")[0].toInt();

  // Now we know more detail about how many lines should be here
  if (lines.size() < 7 + numSpecialPoints)
    return printAndReturnFalse("Too few lines of data in band data!");

  ++ind;
  if (!lines[ind].contains("k points"))
    return printAndReturnFalse("k points missing");
  size_t numKPoints = lines[ind].split(" ")[0].toInt();

  ++ind;
  if (!lines[ind].contains("orbitals in"))
    return printAndReturnFalse("orbitals in missing");;
  size_t numOrbitals = lines[ind].split(" ")[0].toInt();

  for (size_t i = 0; i < numSpecialPoints; ++i) {
    ++ind;
    specialKPoint kp;
    if (lines[ind].split(" ").size() < 4)
      return printAndReturnFalse("Special points line too small");

    kp.label = lines[ind].split(" ")[0];
    // If the label is "GM", use "??" instead
    if (kp.label.toLower() == "gm")
      kp.label = QString::fromUtf8("??");
    kp.coords = Vector3(lines[ind].split(" ")[1].toDouble(),
                        lines[ind].split(" ")[2].toDouble(),
                        lines[ind].split(" ")[3].toDouble());
    specialKPoints.append(kp);
  }

  ++ind;
  if (!lines[ind].contains("Begin band data"))
    return printAndReturnFalse("Begin band data missing");

  // This should be equal in size to the number of orbitals
  bands.reserve(numOrbitals);
  for (size_t i = 0; i < numOrbitals; ++i) {
    bands.append(band());
    // This is how many points we should have in total
    bands[i].reserve(numKPoints * (numSpecialPoints - 1) + 1);
  }
  // This is how many points we should have in total
  kpoints.reserve(numKPoints * (numSpecialPoints - 1) + 1);

  ++ind;
  while (true) {
    // Did we make it to the end without finishing the band data?
    if (ind >= lines.size())
      return printAndReturnFalse("END_BAND_DATA is missing!");

    // Did we make it to the end?
    if (lines[ind].contains("END_BAND_DATA"))
      break;

    // Read the k-point info
    if (!lines[ind].contains("K point") ||
        lines[ind].split(" ").size() < 6) {
      return printAndReturnFalse("K point missing");
    }

    kpoint kp = Vector3(lines[ind].split(" ")[3].toDouble(),
                        lines[ind].split(" ")[4].toDouble(),
                        lines[ind].split(" ")[5].toDouble());
    kpoints.append(kp);

    // Get the orbital energies
    for (size_t j = 0; j < numOrbitals; ++j) {
      ++ind;
      bands[j].append(lines[ind].trimmed().toDouble());
    }
    ++ind;
  }

  // We made it!
  return true;
}

bool YaehmopOut::getFermiLevelFromDOSData(const QString& data,
                                          double& fermi)
{
  QStringList lines = data.split(QRegExp("[\r\n]"), QString::SkipEmptyParts);

  for (size_t i = 0; i < lines.size(); ++i) {
    if (lines[i].contains("Fermi_Energy")) {
      QStringList lineSplit = lines[i].split(" ", QString::SkipEmptyParts);

      if (lineSplit.size() != 2) {
        qDebug() << "Error obtaining Fermi level in " << __FUNCTION__;
        return false;
      }

      bool ok = false;
      double fermiEnergy = lineSplit[1].toFloat(&ok);
      if (!ok) {
        qDebug() << "Error reading Fermi level in " << __FUNCTION__;
        return false;
      }

      fermi = fermiEnergy;
      return true;
    }
  }
  return false;
}

bool YaehmopOut::readTotalDOSData(const QString& data,
                                  QVector<double>& densities,
                                  QVector<double>& energies)
{
  densities.clear();
  energies.clear();

  QStringList lines = data.split(QRegExp("[\r\n]"), QString::SkipEmptyParts);

  while (!lines.isEmpty() && !lines[0].contains("TOTAL DENSITY OF STATES"))
    lines.removeFirst();

  if (lines.isEmpty())
    return printAndReturnFalse("DOS Data not found in readTotalDOSData!");

  size_t ind = 1;

  // The next line should contain <num> states are present
  if (!lines[ind].contains("states are present"))
    return printAndReturnFalse("Number of states present is missing!");

  size_t numDensities = lines[ind].toInt();
  densities.reserve(numDensities);
  energies.reserve(numDensities);

  ++ind;
  // Next line should contain BEGIN CURVE
  if (!lines[ind].contains("BEGIN CURVE"))
    return printAndReturnFalse("BEGIN CURVE is missing!");

  ++ind;
  while (!lines[ind].contains("END CURVE")) {
    QStringList splitLine = lines[ind].split(" ", QString::SkipEmptyParts);
    if (splitLine.size() != 2)
      return printAndReturnFalse("Total DOS data is incomplete!");

    bool ok = true;
    double density = splitLine[0].toFloat(&ok);
    if (!ok)
      return printAndReturnFalse("Invalid number in total DOS data!");

    double energy = splitLine[1].toFloat(&ok);
    if (!ok)
      return printAndReturnFalse("Invalid number in total DOS data!");

    densities.append(density);
    energies.append(energy);
    ++ind;
  }

  // We made it!
  return true;
}

bool YaehmopOut::readProjDOSData(const QString& data,
                                 QVector<QVector<double> >& densities,
                                 QVector<QVector<double> >& energies)
{
  densities.clear();
  energies.clear();

  QStringList lines = data.split(QRegExp("[\r\n]"), QString::SkipEmptyParts);

  while (!lines.isEmpty() &&
         !lines[0].contains("PROJECTED DENSITY OF STATES")) {
    lines.removeFirst();
  }

  if (lines.isEmpty()) {
    return printAndReturnFalse("Projected Data not found in "
                               "readProjDOSData!");
  }

  while (!lines.isEmpty()) {
    QVector<double> tmpDensities, tmpEnergies;
    size_t ind = 1;

    // The next line should contain <num> states are present
    if (!lines[ind].contains("states are present"))
      return printAndReturnFalse("Number of states present is missing!");

    size_t numDensities = lines[ind].toInt();
    tmpDensities.reserve(numDensities);
    tmpEnergies.reserve(numDensities);

    // There is an extra line after this one with the atom info
    ++ind;
    ++ind;
    // Next line should contain BEGIN CURVE
    if (!lines[ind].contains("BEGIN CURVE"))
      return printAndReturnFalse("BEGIN CURVE is missing!");

    ++ind;
    while (!lines[ind].contains("END CURVE")) {
      QStringList splitLine = lines[ind].split(" ", QString::SkipEmptyParts);
      if (splitLine.size() != 2)
        return printAndReturnFalse("Projected DOS data is incomplete!");

      bool ok = true;
      double density = splitLine[0].toFloat(&ok);
      if (!ok)
        return printAndReturnFalse("Invalid number in projected DOS data!");

      double energy = splitLine[1].toFloat(&ok);
      if (!ok)
        return printAndReturnFalse("Invalid number in projected DOS data!");

      tmpDensities.append(density);
      tmpEnergies.append(energy);
      ++ind;
    }
    densities.append(tmpDensities);
    energies.append(tmpEnergies);
    // Scroll past this "PROJECTED DENSITY OF STATES" line
    if (!lines.isEmpty() &&
        lines[0].contains("PROJECTED DENSITY OF STATES")) {
      lines.removeFirst();
    }
    // Get to the next set of projected DOS data if it exists
    while (!lines.isEmpty() &&
           !lines[0].contains("PROJECTED DENSITY OF STATES")) {
      lines.removeFirst();
    }
  }

  // We made it!
  return true;
}

bool YaehmopOut::readCOOPData(const QString& data,
                              QStringList& titles,
                              QVector<QVector<double> >& coops,
                              QVector<QVector<double> >& energies)
{
  coops.clear();
  energies.clear();

  QStringList lines = data.split(QRegExp("[\r\n]"), QString::SkipEmptyParts);

  while (!lines.isEmpty() &&
         !lines[0]
           .contains("COOP (Crystal Orbital Overlap Population) results")) {
    lines.removeFirst();
  }

  if (lines.isEmpty()) {
    return printAndReturnFalse("COOP Data not found in readCOOPData!");
  }

  lines.removeFirst();

  // The next line should contain <num> curves will be generated
  if (!lines[0].contains("curves will be generated"))
    return printAndReturnFalse("Number of curves generated is missing!");

  size_t numCoops = lines[0].split(" ", QString::SkipEmptyParts)[0].toInt();
  size_t numCoopsSoFar = 0;

  while (numCoopsSoFar != numCoops && !lines.isEmpty()) {
    QVector<double> tmpCoops, tmpEnergies;
    tmpCoops.reserve(numCoops);
    tmpEnergies.reserve(numCoops);

    size_t ind = 1;

    // This line should contain title info
    if (!lines[ind].contains("Contributions to this COOP are:"))
      return printAndReturnFalse("Contributions to this COOP line is missing!");

    ++ind;
    QString title;

    while (ind != lines.size() && !lines[ind].contains("BEGIN CURVE")) {
      title += lines[ind];
      ++ind;
    }

    titles.append(title);

    if (ind == lines.size())
      return printAndReturnFalse("Missing 'BEGIN CURVE' in COOP data!");

    ++ind;
    while (ind != lines.size() && !lines[ind].contains("END CURVE")) {
      QStringList splitLine = lines[ind].split(" ", QString::SkipEmptyParts);
      if (splitLine.size() != 2)
        return printAndReturnFalse("COOP data is incomplete!");

      bool ok = true;
      double coop = splitLine[0].toFloat(&ok);
      if (!ok)
        return printAndReturnFalse("Invalid number in COOP data!");

      double energy = splitLine[1].toFloat(&ok);
      if (!ok)
        return printAndReturnFalse("Invalid number in COOP data!");

      tmpCoops.append(coop);
      tmpEnergies.append(energy);
      ++ind;
    }
    coops.append(tmpCoops);
    energies.append(tmpEnergies);
    ++numCoopsSoFar;
    if (numCoopsSoFar != numCoops) {
      // Get to the next set of COOP data if it exists
      while (!lines.isEmpty() &&
             !lines[0].contains("END CURVE")) {
        lines.removeFirst();
      }
      if (lines.isEmpty())
        return printAndReturnFalse("Could not find all COOP data!");
    }
  }

  // We made it!
  return true;
}
