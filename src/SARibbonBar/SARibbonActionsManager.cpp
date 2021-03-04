#include "SARibbonActionsManager.h"
#include <QMap>
#include <QHash>
#include <QDebug>


class SARibbonActionsManagerPrivate
{
public:
    SARibbonActionsManager *mParent;
    QMap<int, QList<QAction *> > mTagToActions;     ///< tag : QList<QAction*>
    QMap<int, QList<QString> > mTagToActionKeys;    ///< tag : keys
    QMap<int, QString> mTagToName;                  ///< tag对应的名字
    QHash<QString, QAction *> mKeyToAction;         ///< key对应action
    QMap<QAction *, QString> mActionToKey;          ///< action对应key
    int mSale;                                      ///< 盐用于生成固定的id，在用户不主动设置key时，id基于msale生成，只要SARibbonActionsManager的调用registeAction顺序不变，生成的id都不变，因为它是基于自增实现的
    SARibbonActionsManagerPrivate(SARibbonActionsManager *p);
};

SARibbonActionsManagerPrivate::SARibbonActionsManagerPrivate(SARibbonActionsManager *p)
    : mParent(p)
    , mSale(0)
{
}


SARibbonActionsManager::SARibbonActionsManager(QObject *p) : QObject(p)
    , m_d(new SARibbonActionsManagerPrivate(this))
{
}


SARibbonActionsManager::~SARibbonActionsManager()
{
    delete m_d;
}


/**
 * @brief 设置tag对应的名字，通过这个可以得到tag和文本的映射
 * @param tag
 * @param name
 * @note 在支持多语言的环境下，在语言切换时需要重新设置，以更新名字
 */
void SARibbonActionsManager::setTagName(int tag, const QString& name)
{
    m_d->mTagToName[tag] = name;
}


/**
 * @brief 获取tag对应的中文名字
 * @param tag
 * @return
 */
QString SARibbonActionsManager::tagName(int tag) const
{
    return (m_d->mTagToName.value(tag, ""));
}


/**
 * @brief 把action注册到管理器中，实现action的管理
 * @param act
 * @param tag tag是可以按照位进行叠加，见 @ref ActionTag 如果
 * 要定义自己的标签，建议定义大于@ref ActionTag::UserDefineActionTag 的值，
 * registeAction的tag是直接记录进去的，如果要多个标签并存，在registe之前先或好tag
 * @param key key是action对应的key，一个key只对应一个action，是查找action的关键
 * ,默认情况为一个QString(),这时key是QAction的objectName
 * @note 同一个action多次注册不同的tag可以通过tag索引到action，但通过action只能索引到最后一个注册的tag
 * @note tag的新增会触发actionTagChanged信号
 */
bool SARibbonActionsManager::registeAction(QAction *act, int tag, const QString& key)
{
    if (nullptr == act) {
        return (false);
    }
    QString k = key;

    if (k.isEmpty()) {
        k = QString("id_%1_%2").arg(m_d->mSale++).arg(act->objectName());
    }
    if (m_d->mKeyToAction.contains(k)) {
        qWarning() << "key: " << k << " have been exist,you can set key in an unique value when use SARibbonActionsManager::registeAction";
        return (false);
    }
    m_d->mKeyToAction[k] = act;
    m_d->mActionToKey[act] = k;
    //记录tag 对 action
    bool isneedemit = !(m_d->mTagToActions.contains(tag));//记录是否需要发射信号

    m_d->mTagToActions[tag].append(act);
    m_d->mTagToActionKeys[tag].append(k);//key也记录
    //绑定槽
    connect(act, &QObject::destroyed, this, &SARibbonActionsManager::onActionDestroyed);
    if (isneedemit) {
        //说明新增tag
        emit actionTagChanged(tag, false);
    }
    return (true);
}


/**
 * @brief 取消action的注册
 *
 * 如果tag对应的最后一个action被撤销，tag也将一块删除
 * @param act
 * @note tag的删除会触发actionTagChanged信号
 * @note 如果action关联了多个tag，这些tag里的action都会被删除，对应的key也同理
 */
void SARibbonActionsManager::unregisteAction(QAction *act, bool enableEmit)
{
    if (nullptr == act) {
        return;
    }
    //绑定槽
    disconnect(act, &QObject::destroyed, this, &SARibbonActionsManager::onActionDestroyed);
    removeAction(act, enableEmit);
}


/**
 * @brief 移除action
 *
 * 仅移除内存内容
 * @param act
 * @param enableEmit
 */
void SARibbonActionsManager::removeAction(QAction *act, bool enableEmit)
{
    QList<int> deletedTags;                         //记录删除的tag，用于触发actionTagChanged
    QMap<int, QList<QAction *> > tagToActions;      ///< tag : QList<QAction*>
    QMap<int, QList<QString> > tagToActionKeys;     ///< tag : keys

    for (auto i = m_d->mTagToActions.begin(); i != m_d->mTagToActions.end(); ++i)
    {
        //把不是act的内容转移到tagToActions和tagToActionKeys中，之后再和m_d里的替换
        auto k = m_d->mTagToActionKeys.find(i.key());
        auto tmpi = tagToActions.insert(i.key(), QList<QAction *>());
        auto tmpk = tagToActionKeys.insert(i.key(), QList<QString>());
        int count = 0;
        for (int j = 0; j < i.value().size(); ++j)
        {
            if (i.value()[j] != act) {
                tmpi.value().append(act);
                tmpk.value().append(k.value()[j]);
                ++count;
            }
        }
        if (0 == count) {
            //说明这个tag没有内容
            tagToActions.erase(tmpi);
            tagToActionKeys.erase(tmpk);
            deletedTags.append(i.key());
        }
    }
    //删除mKeyToAction
    QString key = m_d->mActionToKey.value(act);

    m_d->mActionToKey.remove(act);
    m_d->mKeyToAction.remove(key);

    //置换
    m_d->mTagToActions.swap(tagToActions);
    m_d->mTagToActionKeys.swap(tagToActionKeys);
    //发射信号
    if (enableEmit) {
        for (int tagdelete : deletedTags)
        {
            emit actionTagChanged(tagdelete, true);
        }
    }
}


/**
 * @brief 过滤得到actions对应的引用，实际是一个迭代器
 * @param tag
 * @return
 */
SARibbonActionsManager::ActionRef SARibbonActionsManager::filter(int tag)
{
    return (m_d->mTagToActions.find(tag));
}


/**
 * @brief 判断返回的ActionRefs是否有效，如果没有tag，返回的ActionRefs是无效的
 * @param r
 * @return 有效返回true
 */
bool SARibbonActionsManager::isActionRefsValid(SARibbonActionsManager::ActionRef r) const
{
    return (r != m_d->mTagToActions.end());
}


/**
 * @brief 直接得到一个无效的ActionRefs
 * @return
 */
SARibbonActionsManager::ActionRef SARibbonActionsManager::invalidActionRefs() const
{
    return (m_d->mTagToActions.end());
}


/**
 * @brief 获取所有的标签
 * @return
 */
QList<int> SARibbonActionsManager::actionTags() const
{
    return (m_d->mTagToActions.keys());
}


/**
 * @brief 通过key获取action
 * @param key
 * @return 如果没有key，返回nullptr
 */
QAction *SARibbonActionsManager::action(const QString& key) const
{
    return (m_d->mKeyToAction.value(key, nullptr));
}


/**
 * @brief 通过action找到key
 * @param act
 * @return 如果找不到，返回QString()
 */
QString SARibbonActionsManager::key(QAction *act) const
{
    return (m_d->mActionToKey.value(act, QString()));
}


/**
 * @brief 返回所有管理的action数
 * @return
 */
int SARibbonActionsManager::count() const
{
    return (m_d->mKeyToAction.size());
}


/**
 * @brief action 被delete时，将触发此槽把管理的action删除
 * @param o
 * @note 这个函数不会触发actionTagChanged信号
 */
void SARibbonActionsManager::onActionDestroyed(QObject *o)
{
    QAction *act = static_cast<QAction *>(o);

    removeAction(act, false);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
/// SARibbonActionsModel
////////////////////////////////////////////////////////////////////////////////////////////////////////
class SARibbonActionsModelPrivete
{
public:
    SARibbonActionsManagerModel *mParent;
    SARibbonActionsManager *mMgr;
    int mTag;
    SARibbonActionsManager::ActionRef mActionRef;
    SARibbonActionsModelPrivete(SARibbonActionsManagerModel *m);
    bool isValidRef() const;
    void updateRef();
    int count() const;
    QAction *at(int index);
    bool isNull() const;
};

SARibbonActionsModelPrivete::SARibbonActionsModelPrivete(SARibbonActionsManagerModel *m)
    : mParent(m)
    , mMgr(nullptr)
    , mTag(SARibbonActionsManager::CommonlyUsedActionTag)
{
}


bool SARibbonActionsModelPrivete::isValidRef() const
{
    if (isNull()) {
        return (false);
    }
    return (mMgr->isActionRefsValid(mActionRef));
}


void SARibbonActionsModelPrivete::updateRef()
{
    if (isNull()) {
        return;
    }
    mActionRef = mMgr->filter(mTag);
}


int SARibbonActionsModelPrivete::count() const
{
    if (isNull()) {
        return (0);
    }
    if (isValidRef()) {
        return (mActionRef.value().size());
    }
    return (0);
}


QAction *SARibbonActionsModelPrivete::at(int index)
{
    if (isNull()) {
        return (nullptr);
    }
    if (!isValidRef()) {
        return (nullptr);
    }
    if (index >= count()) {
        return (nullptr);
    }
    return (mActionRef.value().at(index));
}


bool SARibbonActionsModelPrivete::isNull() const
{
    return (mMgr == nullptr);
}


SARibbonActionsManagerModel::SARibbonActionsManagerModel(QObject *p) : QAbstractListModel(p)
    , m_d(new SARibbonActionsModelPrivete(this))
{
}


SARibbonActionsManagerModel::SARibbonActionsManagerModel(SARibbonActionsManager *m, QObject *p) : QAbstractListModel(p)
    , m_d(new SARibbonActionsModelPrivete(this))
{
    setupActionsManager(m);
}


SARibbonActionsManagerModel::~SARibbonActionsManagerModel()
{
    delete m_d;
}


int SARibbonActionsManagerModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {//非顶层
        return (0);
    }
    //顶层
    return (m_d->count());
}


QVariant SARibbonActionsManagerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section);
    if (role != Qt::DisplayRole) {
        return (QVariant());
    }
    if (Qt::Horizontal == orientation) {
        return (tr("action name"));
    }
    return (QVariant());
}


Qt::ItemFlags SARibbonActionsManagerModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return (Qt::NoItemFlags);
    }
    return (Qt::ItemIsSelectable | Qt::ItemIsEnabled);
}


QVariant SARibbonActionsManagerModel::data(const QModelIndex& index, int role) const
{
    QAction *act = indexToAction(index);

    if (nullptr == act) {
        return (QVariant());
    }

    switch (role)
    {
    case Qt::DisplayRole:
        return (act->text());

    case Qt::DecorationRole:
        return (act->icon());

    default:
        break;
    }
    return (QVariant());
}


void SARibbonActionsManagerModel::setFilter(int tag)
{
    m_d->mTag = tag;
    update();
}


void SARibbonActionsManagerModel::update()
{
    beginResetModel();
    m_d->updateRef();
    endResetModel();
}


void SARibbonActionsManagerModel::setupActionsManager(SARibbonActionsManager *m)
{
    m_d->mMgr = m;
    m_d->mTag = SARibbonActionsManager::CommonlyUsedActionTag;
    m_d->mActionRef = m->filter(m_d->mTag);
    connect(m, &SARibbonActionsManager::actionTagChanged
        , this, &SARibbonActionsManagerModel::onActionTagChanged);
    update();
}


void SARibbonActionsManagerModel::uninstallActionsManager()
{
    if (!m_d->isNull()) {
        disconnect(m_d->mMgr, &SARibbonActionsManager::actionTagChanged
            , this, &SARibbonActionsManagerModel::onActionTagChanged);
        m_d->mMgr = nullptr;
        m_d->mTag = SARibbonActionsManager::CommonlyUsedActionTag;
    }
    update();
}


QAction *SARibbonActionsManagerModel::indexToAction(QModelIndex index) const
{
    if (!index.isValid()) {
        return (nullptr);
    }
    if (index.row() >= m_d->count()) {
        return (nullptr);
    }
    return (m_d->at(index.row()));
}


void SARibbonActionsManagerModel::onActionTagChanged(int tag, bool isdelete)
{
    if (isdelete && (tag == m_d->mTag)) {
        m_d->mTag = SARibbonActionsManager::InvalidActionTag;
        update();
    }else{
        if (tag == m_d->mTag) {
            update();
        }
    }
}