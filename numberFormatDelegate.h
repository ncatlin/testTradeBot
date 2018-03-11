/*
Copyright 2018 Nia Catlin

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#pragma once
#ifndef NUMBERFORMATDELEGATE_H
#define NUMBERFORMATDELEGATE_H

#include <QStyledItemDelegate>

class NumberFormatDelegate : public QStyledItemDelegate
{
	Q_OBJECT
public:
	explicit NumberFormatDelegate(QObject *parent = 0);
	virtual QString displayText(const QVariant &value, const QLocale &locale) const;

signals:

	public slots :

};

#endif // NUMBERFORMATDELEGATE_H
