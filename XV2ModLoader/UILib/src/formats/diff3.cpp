// diff3 (see diff3.h): longest common subsequence matching of the base
// against each side, then the regions between lines all three share are
// resolved one by one.
#include "diff3.h"
#include <cstdint>

namespace xv2ui {
namespace {

const size_t kNone = SIZE_MAX;
// The line matcher's table is lines(a) x lines(b) u16s; past this many
// cells (32 MB) the merge gives up instead (the biggest game method has
// about 3000 instructions, well inside).
const size_t kMaxCells = 16u * 1024 * 1024;

// For each line of `a`, the index of the line of `b` it is matched to in a
// longest common subsequence (kNone when unmatched). False when too big.
bool Match(const std::vector<std::string>& a, const std::vector<std::string>& b, std::vector<size_t>& aToB)
{
    aToB.assign(a.size(), kNone);
    // equal head and tail lines match directly; only the middle needs the table
    size_t head = 0;
    while (head < a.size() && head < b.size() && a[head] == b[head]) { aToB[head] = head; ++head; }
    size_t tail = 0;
    while (tail < a.size() - head && tail < b.size() - head && a[a.size() - 1 - tail] == b[b.size() - 1 - tail]) {
        aToB[a.size() - 1 - tail] = b.size() - 1 - tail;
        ++tail;
    }
    const size_t rows = a.size() - head - tail, columns = b.size() - head - tail;
    if (rows == 0 || columns == 0) return true;
    if ((rows + 1) * (columns + 1) > kMaxCells) return false;
    // length[r][c] = LCS length of a[head+r..] and b[head+c..], saturating at 0xffff
    std::vector<uint16_t> length((rows + 1) * (columns + 1), 0);
    auto at = [&](size_t r, size_t c) -> uint16_t& { return length[r * (columns + 1) + c]; };
    for (size_t r = rows; r-- > 0;)
        for (size_t c = columns; c-- > 0;) {
            if (a[head + r] == b[head + c]) at(r, c) = (uint16_t)(at(r + 1, c + 1) == 0xffff ? 0xffff : at(r + 1, c + 1) + 1);
            else at(r, c) = at(r + 1, c) > at(r, c + 1) ? at(r + 1, c) : at(r, c + 1);
        }
    size_t r = 0, c = 0;
    while (r < rows && c < columns) {
        if (a[head + r] == b[head + c]) { aToB[head + r] = head + c; ++r; ++c; }
        else if (at(r + 1, c) >= at(r, c + 1)) ++r;
        else ++c;
    }
    return true;
}

bool SameLines(const std::vector<std::string>& a, size_t aStart, size_t aEnd, const std::vector<std::string>& b,
               size_t bStart, size_t bEnd)
{
    if (aEnd - aStart != bEnd - bStart) return false;
    for (size_t i = 0; i < aEnd - aStart; ++i)
        if (a[aStart + i] != b[bStart + i]) return false;
    return true;
}

}  // namespace

bool Diff3(const std::vector<std::string>& base, const std::vector<std::string>& theirs,
           const std::vector<std::string>& ours, std::vector<Diff3Pick>& picks, std::vector<size_t>& theirsPosition,
           std::vector<size_t>& oursPosition, bool* bothInserted)
{
    if (bothInserted) *bothInserted = false;
    picks.clear();
    theirsPosition.assign(theirs.size() + 1, kNone);
    oursPosition.assign(ours.size() + 1, kNone);
    std::vector<size_t> baseToTheirs, baseToOurs;
    if (!Match(base, theirs, baseToTheirs) || !Match(base, ours, baseToOurs)) return false;

    // picks lines [start, end) of one side; the other side's region (if any)
    // is placed where they start
    auto take = [&](bool fromTheirs, size_t start, size_t end) {
        std::vector<size_t>& position = fromTheirs ? theirsPosition : oursPosition;
        for (size_t i = start; i < end; ++i) {
            position[i] = picks.size();
            picks.push_back({fromTheirs, i});
        }
    };
    size_t baseAt = 0, theirsAt = 0, oursAt = 0;
    for (;;) {
        // the next base line both sides kept (a stable line)
        size_t stable = baseAt;
        while (stable < base.size() && (baseToTheirs[stable] == kNone || baseToOurs[stable] == kNone)) ++stable;
        const bool atEnd = stable == base.size();
        const size_t theirsEnd = atEnd ? theirs.size() : baseToTheirs[stable];
        const size_t oursEnd = atEnd ? ours.size() : baseToOurs[stable];
        const size_t regionStart = picks.size();

        // the region before it: whichever side changed it wins
        if (SameLines(theirs, theirsAt, theirsEnd, base, baseAt, stable)) {
            take(false, oursAt, oursEnd);   // theirs' lines here are gone: kNone
        } else if (SameLines(ours, oursAt, oursEnd, base, baseAt, stable)) {
            take(true, theirsAt, theirsEnd);
            for (size_t i = oursAt; i < oursEnd; ++i) oursPosition[i] = regionStart;
        } else if (SameLines(theirs, theirsAt, theirsEnd, ours, oursAt, oursEnd)) {
            take(false, oursAt, oursEnd);
            for (size_t i = 0; i < theirsEnd - theirsAt; ++i) theirsPosition[theirsAt + i] = regionStart + i;
        } else if (baseAt == stable) {
            // both only inserted lines here (no base line in between): keep
            // both, ours first - like adding two new constants or calls
            take(false, oursAt, oursEnd);
            take(true, theirsAt, theirsEnd);
            if (bothInserted) *bothInserted = true;
        } else {
            return false;   // both changed it, differently
        }
        if (atEnd) break;
        theirsPosition[theirsEnd] = picks.size();
        take(false, oursEnd, oursEnd + 1);
        baseAt = stable + 1;
        theirsAt = theirsEnd + 1;
        oursAt = oursEnd + 1;
    }
    theirsPosition[theirs.size()] = oursPosition[ours.size()] = picks.size();
    return true;
}

}  // namespace xv2ui
