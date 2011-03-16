// Copyright (C) 2010 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#include <QDebug>

#include "Common/CF.hpp"

#include "GUI/Client/Core/CommitDetails.hpp"
#include "GUI/Client/UI/CommitDetailsDialog.hpp"
#include "GUI/Client/UI/ConfirmCommitDialog.hpp"
#include "GUI/Client/UI/GraphicalValue.hpp"
#include "GUI/Client/Core/TreeThread.hpp"
#include "GUI/Client/Core/CNode.hpp"

#include "GUI/Client/UI/OptionLayout.hpp"

using namespace CF::Common;
using namespace CF::GUI::ClientCore;
using namespace CF::GUI::ClientUI;


OptionLayout::OptionLayout(QWidget * parent)
  : QFormLayout(parent)
{

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

OptionLayout::~OptionLayout()
{
  this->clearOptions();
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void OptionLayout::options(QMap<QString, QString> & options, bool all) const
{
  QMap<QString, GraphicalValue *>::const_iterator it = m_options.begin();

  for( ; it != m_options.end() ; it++)
  {
    GraphicalValue * value = it.value();

    if(all || value->isModified())
      options[ it.key() ] = value->valueString();
  }
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void OptionLayout::commitOpions()
{
  QMap<QString, GraphicalValue *>::const_iterator it = m_options.begin();

  for( ; it != m_options.end() ; it++)
    it.value()->commit();
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void OptionLayout::clearOptions()
{
  QMap<QString, GraphicalValue *>::const_iterator it = m_options.begin();

  while(it != m_options.end())
  {
    delete it.value();
    it++;
  }

  m_options.clear();
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

bool OptionLayout::isModified() const
{
  bool modified = false;

  QMap<QString, GraphicalValue *>::const_iterator it = m_options.begin();

  for( ; it != m_options.end() && !modified ; it++)
    modified = it.value()->isModified();

  return modified;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void OptionLayout::modifiedOptions(CommitDetails & commitDetails) const
{
  QMap<QString, GraphicalValue *>::const_iterator it = m_options.begin();

  while(it != m_options.end())
  {
    GraphicalValue * value = it.value();

    if(value->isModified())
    {
      QString oldValue = value->originalValueString();
      QString newValue = value->valueString();

      commitDetails.setOption(it.key(), oldValue, newValue);
    }

    it++;
  }
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void OptionLayout::addOption(CF::Common::Option::ConstPtr option)
{
  GraphicalValue * value = GraphicalValue::createFromOption(option);
  QString name(option->name().c_str());

  m_options[name] = value;

  value->setToolTip(option->description().c_str());

  addRow(name + ':', value);

  // forward the signal
  connect(value, SIGNAL(valueChanged()), this, SIGNAL(valueChanged()));
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

bool OptionLayout::hasOptions() const
{
  return !m_options.isEmpty();
}
