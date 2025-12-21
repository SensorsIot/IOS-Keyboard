import Foundation

/// Result of computing a text diff
struct TextDiff {
    let backspaces: Int
    let insert: String
}

/// Service for computing minimal text diffs between speech recognition updates
class TextDiffService {
    /// The last text that was successfully transmitted
    private var lastSentText: String = ""

    /// Reset the diff state (call when starting a new recording session)
    func reset() {
        lastSentText = ""
    }

    /// Compute the minimal diff between the last sent text and new text
    /// - Parameter newText: The new recognized text
    /// - Returns: A TextDiff with backspaces to delete and text to insert
    func computeDiff(newText: String) -> TextDiff {
        // Find common prefix length
        let commonPrefixLength = findCommonPrefixLength(lastSentText, newText)

        // Calculate backspaces needed (chars to delete from old text after common prefix)
        let backspaces = lastSentText.count - commonPrefixLength

        // Calculate text to insert (chars from new text after common prefix)
        let insertStartIndex = newText.index(newText.startIndex, offsetBy: commonPrefixLength)
        let insert = String(newText[insertStartIndex...])

        // Update last sent text
        lastSentText = newText

        return TextDiff(backspaces: backspaces, insert: insert)
    }

    /// Find the length of the common prefix between two strings
    private func findCommonPrefixLength(_ a: String, _ b: String) -> Int {
        let aChars = Array(a)
        let bChars = Array(b)
        let minLength = min(aChars.count, bChars.count)

        var commonLength = 0
        for i in 0..<minLength {
            if aChars[i] == bChars[i] {
                commonLength += 1
            } else {
                break
            }
        }

        return commonLength
    }

    /// Get the currently tracked "sent" text (for debugging/display)
    var currentSentText: String {
        return lastSentText
    }
}
