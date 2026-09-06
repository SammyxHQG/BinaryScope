#include "HashCalculator.h"
#include <QCryptographicHash>
#include <algorithm>
namespace bs {
Hashes HashCalculator::calculate(std::span<const std::uint8_t> bytes) {
    QCryptographicHash sha(QCryptographicHash::Sha256), md5(QCryptographicHash::Md5);
    constexpr std::size_t chunk = 1024 * 1024;
    for (std::size_t offset = 0; offset < bytes.size(); offset += chunk) {
        const QByteArrayView view(reinterpret_cast<const char*>(bytes.data() + offset),
                                  static_cast<qsizetype>(std::min(chunk, bytes.size() - offset)));
        sha.addData(view);
        md5.addData(view);
    }
    return {QString::fromLatin1(sha.result().toHex()), QString::fromLatin1(md5.result().toHex())};
}
} // namespace bs
