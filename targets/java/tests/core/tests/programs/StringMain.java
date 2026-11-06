final class StringMain {
    static long failedIf(double off) {
        if (off > 0.001 || off < 0.0 - 0.001) {
            return 1L;
        }
        return 0L;
    }

    static long main() {
        long failed = 0L;
        failed = failed + failedIf(StringTests.checkEscapes());
        failed = failed + failedIf(StringTests.checkCharEscapes());
        failed = failed + failedIf(StringTests.checkInterpolation());
        failed = failed + failedIf(StringTests.checkInterpolatedMethodCalls());
        failed = failed + failedIf(StringTests.checkTextBlock());
        failed = failed + failedIf(StringTests.checkTextBlockWithoutFinalNewline());
        failed = failed + failedIf(StringTests.checkTextBlockKeepsIndentOfMultiLineValue());
        failed = failed + failedIf(StringTests.checkStrOfValues());
        failed = failed + failedIf(StringTests.checkCharAt());
        failed = failed + failedIf(StringTests.checkSliceAndFind());
        failed = failed + failedIf(StringTests.checkSliceClamps());
        failed = failed + failedIf(StringTests.checkFindFrom());
        failed = failed + failedIf(StringTests.checkListLiteralAndForIn());
        failed = failed + failedIf(StringTests.checkSplitAndJoin());
        failed = failed + failedIf(StringTests.checkTrimAndContains());
        failed = failed + failedIf(StringTests.checkStartsAndEnds());
        failed = failed + failedIf(StringTests.checkReplaceRepeatIndent());
        failed = failed + failedIf(StringTests.checkLines());
        failed = failed + failedIf(StringTests.checkFill());
        failed = failed + failedIf(StringTests.checkStringBuilder());
        return failed;
    }

    public static void main(String[] args) {
        System.exit((int) main());
    }
}
