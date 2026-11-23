#include "scene/ItemNameService.h"

#include "items/CanvasItem.h"

bool ItemNameService::IsAvailable(const QList<CanvasItem*>& items, const QString& name, const CanvasItem* except)
{
    const QString candidate = name.trimmed();
    if (candidate.isEmpty()) {
        return true;
    }
    for (CanvasItem* item : items) {
        if (!item || item == except) {
            continue;
        }
        if (item->Name().trimmed().compare(candidate, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }
    return true;
}

QString ItemNameService::Unique(const QList<CanvasItem*>& items, const QString& base, const CanvasItem* except)
{
    const QString typeName = base.trimmed();
    const QString prefix = typeName + QLatin1Char(' ');
    int maxSuffix = 0;
    for (CanvasItem* item : items) {
        if (!item || item == except) {
            continue;
        }
        const QString existing = item->Name();
        if (!existing.startsWith(prefix, Qt::CaseInsensitive)) {
            continue;
        }
        bool ok = false;
        const int value = existing.mid(prefix.size()).toInt(&ok);
        if (ok && value > 0) {
            maxSuffix = qMax(maxSuffix, value);
        }
    }
    return QStringLiteral("%1 %2").arg(typeName).arg(maxSuffix + 1);
}

QString ItemNameService::UniqueFor(const QList<CanvasItem*>& items, const QString& requested, const CanvasItem* except)
{
    static const QSet<QString> RESERVED;
    return UniqueFor(items, requested, except, RESERVED);
}

QString ItemNameService::UniqueFor(const QList<CanvasItem*>& items, const QString& requested, const CanvasItem* except,
                                   const QSet<QString>& reserved)
{
    const QString trimmed = requested.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }

    // “Hero panel” → base=Hero panel, suffix=0；“矩形 1” → base=矩形, suffix=1。
    QString base = trimmed;
    int suffix = 0;
    const int split = base.lastIndexOf(QLatin1Char(' '));
    if (split > 0) {
        bool ok = false;
        const int value = base.mid(split + 1).toInt(&ok);
        if (ok && value > 0) {
            base = base.left(split).trimmed();
            suffix = value;
        }
    }
    if (base.isEmpty()) {
        base = trimmed;
    }

    const QString prefix = base + QLatin1Char(' ');
    bool exactBase = false;
    int maxSuffix = suffix;

    auto consider = [&](const QString& customName) {
        const QString existing = customName.trimmed();
        if (existing.isEmpty()) {
            return;
        }
        if (existing.compare(base, Qt::CaseInsensitive) == 0) {
            exactBase = true;
        }
        if (!existing.startsWith(prefix, Qt::CaseInsensitive)) {
            return;
        }
        bool ok = false;
        const int value = existing.mid(prefix.size()).toInt(&ok);
        if (ok && value > 0) {
            maxSuffix = qMax(maxSuffix, value);
        }
    };

    for (CanvasItem* item : items) {
        if (!item || item == except) {
            continue;
        }
        consider(item->Name());
    }
    for (const QString& name : reserved) {
        consider(name);
    }

    int next = qMax(maxSuffix + 1, exactBase ? 2 : 1);
    if (suffix > 0) {
        next = qMax(next, suffix + 1);
    }
    return prefix + QString::number(next);
}

void ItemNameService::AssignAutoNames(const QList<CanvasItem*>& items)
{
    for (CanvasItem* item : items) {
        if (!item) {
            continue;
        }
        const QString existing = item->CustomName().trimmed();
        if (existing.isEmpty()) {
            item->SetName(Unique(items, item->TypeDisplayName(), item));
        } else if (!IsAvailable(items, existing, item)) {
            item->SetName(UniqueFor(items, existing, item));
        }
    }
}
