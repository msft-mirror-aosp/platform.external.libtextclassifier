/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.textclassifier

import android.content.Context
import androidx.collection.LruCache
import androidx.test.ext.junit.runners.AndroidJUnit4
import com.android.textclassifier.common.ModelFile
import com.android.textclassifier.common.ModelType
import com.android.textclassifier.common.TextClassifierSettings
import com.android.textclassifier.testing.FakeContextBuilder
import com.android.textclassifier.testing.TestingDeviceConfig
import com.android.textclassifier.utils.TextClassifierUtils
import com.google.android.textclassifier.AnnotatorModel
import org.junit.Assert
import org.junit.Assume
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.ArgumentMatchers.any
import org.mockito.Mock
import org.mockito.Mockito
import org.mockito.MockitoAnnotations

@RunWith(AndroidJUnit4::class)
class TextClassifierOtpHelperTest {
    @Mock
    private lateinit var modelFileManager: ModelFileManager

    private lateinit var context: Context
    private lateinit var deviceConfig: TestingDeviceConfig
    private lateinit var settings: TextClassifierSettings
    private lateinit var annotatorModelCache: LruCache<ModelFile, AnnotatorModel>
    private lateinit var tcImpl: TextClassifierImpl

    @Before
    fun setup() {
        Assume.assumeTrue(TextClassifierUtils.isOtpClassificationEnabled())

        MockitoAnnotations.initMocks(this)
        this.context =
            FakeContextBuilder()
                .setAllIntentComponent(FakeContextBuilder.DEFAULT_COMPONENT)
                .setAppLabel(FakeContextBuilder.DEFAULT_COMPONENT.packageName, "Test app")
                .build()
        this.deviceConfig = TestingDeviceConfig()
        this.settings = TextClassifierSettings(deviceConfig, /* isWear= */ false)
        this.annotatorModelCache = LruCache(2)
        this.tcImpl =
            TextClassifierImpl(context, settings, modelFileManager, annotatorModelCache)

        Mockito.`when`(
            modelFileManager.findBestModelFile(
                ArgumentMatchers.eq(ModelType.ANNOTATOR),
                any(),
                any()
            )
        )
            .thenReturn(TestDataUtils.getTestAnnotatorModelFileWrapped())
        Mockito.`when`(
            modelFileManager.findBestModelFile(
                ArgumentMatchers.eq(ModelType.LANG_ID),
                any(),
                any()
            )
        )
            .thenReturn(TestDataUtils.getLangIdModelFileWrapped())
        Mockito.`when`(
            modelFileManager.findBestModelFile(
                ArgumentMatchers.eq(ModelType.ACTIONS_SUGGESTIONS),
                any(),
                any()
            )
        )
            .thenReturn(TestDataUtils.getTestActionsModelFileWrapped())
    }

    private fun containsOtp(text: String): Boolean {
        return TextClassifierOtpHelper.containsOtp(
            text,
            this.tcImpl,
        )
    }

    @Test
    fun testOtpDetection() {
        Assert.assertFalse(containsOtp("hello"))
        Assert.assertTrue(containsOtp("Your OTP code is 123456"))
    }

    @Test
    fun testContainsOtpLikePattern_length() {
        val tooShortAlphaNum = "123G"
        val tooShortNumOnly = "123"
        val minLenAlphaNum = "123G5"
        val minLenNumOnly = "1235"
        val twoTriplets = "123 456"
        val tooShortTriplets = "12 345"
        val maxLen = "123456F8"
        val tooLong = "123T56789"

        Assert.assertTrue(TextClassifierOtpHelper.containsOtpLikePattern(minLenAlphaNum))
        Assert.assertTrue(TextClassifierOtpHelper.containsOtpLikePattern(minLenNumOnly))
        Assert.assertTrue(TextClassifierOtpHelper.containsOtpLikePattern(maxLen))
        Assert.assertFalse(
            "$tooShortAlphaNum is too short",
            TextClassifierOtpHelper.containsOtpLikePattern(tooShortAlphaNum)
        )
        Assert.assertFalse(
            "$tooShortNumOnly is too short",
            TextClassifierOtpHelper.containsOtpLikePattern(tooShortNumOnly)
        )
        Assert.assertFalse(
            "$tooLong is too long",
            TextClassifierOtpHelper.containsOtpLikePattern(tooLong)
        )
        Assert.assertTrue(TextClassifierOtpHelper.containsOtpLikePattern(twoTriplets))
        Assert.assertFalse(
            "$tooShortTriplets is too short",
            TextClassifierOtpHelper.containsOtpLikePattern(tooShortTriplets)
        )
    }

    @Test
    fun testContainsOtpLikePattern_acceptsNonRomanAlphabeticalChars() {
        val lowercase = "123ķ4"
        val uppercase = "123Ŀ4"
        val ideographicInMiddle = "123码456"

        Assert.assertTrue(TextClassifierOtpHelper.containsOtpLikePattern(lowercase))
        Assert.assertTrue(TextClassifierOtpHelper.containsOtpLikePattern(uppercase))
        Assert.assertFalse(TextClassifierOtpHelper.containsOtpLikePattern(ideographicInMiddle))
    }

    @Test
    fun testContainsOtpLikePattern_dashes() {
        val oneDash = "G-3d523"
        val manyDashes = "G-FD-745"
        val tooManyDashes = "6--7893"
        val oopsAllDashes = "------"

        Assert.assertTrue(TextClassifierOtpHelper.containsOtpLikePattern(oneDash))
        Assert.assertTrue(TextClassifierOtpHelper.containsOtpLikePattern(oneDash))
        Assert.assertTrue(TextClassifierOtpHelper.containsOtpLikePattern(manyDashes))
        Assert.assertFalse(TextClassifierOtpHelper.containsOtpLikePattern(tooManyDashes))
        Assert.assertFalse(TextClassifierOtpHelper.containsOtpLikePattern(oopsAllDashes))
    }

    @Test
    fun testContainsOtpLikePattern_lookaheadMustBeOtpChar() {
        val validLookahead = "g4zy75"
        val spaceLookahead = "GVRXY 2"
        Assert.assertTrue(TextClassifierOtpHelper.containsOtpLikePattern(validLookahead))
        Assert.assertFalse(TextClassifierOtpHelper.containsOtpLikePattern(spaceLookahead))
    }

    @Test
    fun testContainsOtpLikePattern_dateExclusion() {
        val date = "01-01-2001"
        val singleDigitDate = "1-1-2001"
        val twoDigitYear = "1-1-01"
        val dateWithOtpAfter = "1-1-01 is the date of your code T3425"
        val dateWithOtpBefore = "your code 54-234-3 was sent on 1-1-01"
        val otpWithDashesButInvalidDate = "34-58-30"
        val otpWithDashesButInvalidYear = "12-1-3089"

        Assert.assertFalse(TextClassifierOtpHelper.containsOtpLikePattern(date))
        Assert.assertFalse(TextClassifierOtpHelper.containsOtpLikePattern(singleDigitDate))
        Assert.assertFalse(TextClassifierOtpHelper.containsOtpLikePattern(twoDigitYear))

        Assert.assertTrue(TextClassifierOtpHelper.containsOtpLikePattern(dateWithOtpAfter))
        Assert.assertTrue(TextClassifierOtpHelper.containsOtpLikePattern(dateWithOtpBefore))
        Assert.assertTrue(
            TextClassifierOtpHelper.containsOtpLikePattern(otpWithDashesButInvalidDate)
        )
        Assert.assertTrue(
            TextClassifierOtpHelper.containsOtpLikePattern(otpWithDashesButInvalidYear)
        )
    }

    @Test
    fun testContainsOtpLikePattern_phoneExclusion() {
        val parens = "(888) 8888888"
        val allSpaces = "888 888 8888"
        val withDash = "(888) 888-8888"
        val allDashes = "888-888-8888"
        val allDashesWithParen = "(888)-888-8888"

        Assert.assertFalse(TextClassifierOtpHelper.containsOtpLikePattern(parens))
        Assert.assertFalse(TextClassifierOtpHelper.containsOtpLikePattern(allSpaces))
        Assert.assertFalse(TextClassifierOtpHelper.containsOtpLikePattern(withDash))
        Assert.assertFalse(TextClassifierOtpHelper.containsOtpLikePattern(allDashes))
        Assert.assertFalse(TextClassifierOtpHelper.containsOtpLikePattern(allDashesWithParen))
    }

    @Test
    fun testContainsOtp_falsePositiveExclusion() {
        // OTP: [888-8888] falsePositives=[] finalOtpCandidate=[1234]
        Assert.assertTrue(containsOtp("Your OTP is 888-8888"))

        // OTP: [1234, 888-8888] falsePositives=[(888) 888-8888] finalOtpCandidate=[1234]
        Assert.assertTrue(containsOtp("1234 is your OTP, call (888) 888-8888 for more info"))

        // OTP: [888-8888] falsePositives=[(888) 888-8888] finalOtpCandidate=[]
        Assert.assertFalse(containsOtp("Your OTP can't be shared at this point, please call (888) 888-8888"))

        // OTP: [1234, 01-01-2001] falsePositives=[01-01-2001] finalOtpCandidate=[1234]
        Assert.assertTrue(containsOtp("Your OTP code is 1234 and this is sent on 01-01-2001"))

        // OTP: [01-01-2001] falsePositives=[01-01-2001] finalOtpCandidate=[]
        Assert.assertFalse(containsOtp("Your OTP code is null and this is sent on 01-01-2001"))
    }

    @Test
    fun testContainsOtp_mustHaveNumber() {
        val noNums = "TEFHXES"
        Assert.assertFalse(containsOtp(noNums))
    }

    @Test
    fun testContainsOtp_startAndEnd() {
        val noSpaceStart = "your code isG-345821"
        val noSpaceEnd = "your code is G-345821for real"
        val numberSpaceStart = "your code is 4 G-345821"
        val numberSpaceEnd = "your code is G-345821 3"
        val colonStart = "your code is:G-345821"
        val newLineStart = "your code is \nG-345821"
        val quote = "your code is 'G-345821'"
        val doubleQuote = "your code is \"G-345821\""
        val bracketStart = "your code is [G-345821"
        val ideographicStart = "your code is码G-345821"
        val colonStartNumberPreceding = "your code is4:G-345821"
        val periodEnd = "you code is G-345821."
        val parens = "you code is (G-345821)"
        val squareBrkt = "you code is [G-345821]"
        val dashEnd = "you code is 'G-345821-'"
        val randomSymbolEnd = "your code is G-345821$"
        val underscoreEnd = "you code is 'G-345821_'"
        val ideographicEnd = "your code is码G-345821码"
        Assert.assertFalse(containsOtp(noSpaceStart))
        Assert.assertFalse(containsOtp(noSpaceEnd))
        Assert.assertFalse(containsOtp(numberSpaceStart))
        Assert.assertFalse(containsOtp(numberSpaceEnd))
        Assert.assertFalse(containsOtp(colonStartNumberPreceding))
        Assert.assertFalse(containsOtp(dashEnd))
        Assert.assertFalse(containsOtp(underscoreEnd))
        Assert.assertFalse(containsOtp(randomSymbolEnd))
        Assert.assertTrue(containsOtp(colonStart))
        Assert.assertTrue(containsOtp(newLineStart))
        Assert.assertTrue(containsOtp(quote))
        Assert.assertTrue(containsOtp(doubleQuote))
        Assert.assertTrue(containsOtp(bracketStart))
        Assert.assertTrue(containsOtp(ideographicStart))
        Assert.assertTrue(containsOtp(periodEnd))
        Assert.assertTrue(containsOtp(parens))
        Assert.assertTrue(containsOtp(squareBrkt))
        Assert.assertTrue(containsOtp(ideographicEnd))
    }

    @Test
    fun testContainsOtp_multipleFalsePositives() {
        val otp = "code 1543 code"
        val longFp = "888-777-6666"
        val shortFp = "34ess"
        val multipleLongFp = "$longFp something something $longFp"
        val multipleLongFpWithOtpBefore = "$otp $multipleLongFp"
        val multipleLongFpWithOtpAfter = "$multipleLongFp $otp"
        val multipleLongFpWithOtpBetween = "$longFp $otp $longFp"
        val multipleShortFp = "$shortFp something something $shortFp"
        val multipleShortFpWithOtpBefore = "$otp $multipleShortFp"
        val multipleShortFpWithOtpAfter = "$otp $multipleShortFp"
        val multipleShortFpWithOtpBetween = "$shortFp $otp $shortFp"
        Assert.assertFalse(containsOtp(multipleLongFp))
        Assert.assertFalse(containsOtp(multipleShortFp))
        Assert.assertTrue(containsOtp(multipleLongFpWithOtpBefore))
        Assert.assertTrue(containsOtp(multipleLongFpWithOtpAfter))
        Assert.assertTrue(containsOtp(multipleLongFpWithOtpBetween))
        Assert.assertTrue(containsOtp(multipleShortFpWithOtpBefore))
        Assert.assertTrue(containsOtp(multipleShortFpWithOtpAfter))
        Assert.assertTrue(containsOtp(multipleShortFpWithOtpBetween))
    }

    @Test
    fun testContainsOtpCode_nonEnglish() {
        val textWithOtp = "1234 是您的一次性代碼" // 1234 is your one time code
        Assert.assertFalse(containsOtp(textWithOtp))
    }

    @Test
    fun testContainsOtp_englishSpecificRegex() {
        val englishFalsePositive = "This is a false positive 4543"
        val englishContextWords =
            listOf(
                "login",
                "log in",
                "2fa",
                "authenticate",
                "auth",
                "authentication",
                "tan",
                "password",
                "passcode",
                "two factor",
                "two-factor",
                "2factor",
                "2 factor",
                "pin",
                "one time",
            )
        val englishContextWordsCase = listOf("LOGIN", "logIn", "LoGiN")
        // Strings with a context word somewhere in the substring
        val englishContextSubstrings = listOf("pins", "gaping", "backspin")
        val codeInNextSentence = "context word: code. This sentence has the actual value of 434343"
        val codeInNextSentenceTooFar =
            "context word: code. ${"f".repeat(60)} This sentence has the actual value of 434343"
        val codeTwoSentencesAfterContext = "context word: code. One sentence. actual value 34343"
        val codeInSentenceBeforeContext = "34343 is a number. This number is a code"
        val codeInSentenceAfterNewline = "your code is \n 34343"
        val codeTooFarBeforeContext = "34343 ${"f".repeat(60)} code"

        Assert.assertFalse(containsOtp(englishFalsePositive))
        for (context in englishContextWords) {
            val englishTruePositive = "$context $englishFalsePositive"
            Assert.assertTrue(containsOtp(englishTruePositive))
        }
        for (context in englishContextWordsCase) {
            val englishTruePositive = "$context $englishFalsePositive"
            Assert.assertTrue(containsOtp(englishTruePositive))
        }
        for (falseContext in englishContextSubstrings) {
            val anotherFalsePositive = "$falseContext $englishFalsePositive"
            Assert.assertFalse(containsOtp(anotherFalsePositive))
        }
        Assert.assertTrue(containsOtp(codeInNextSentence))
        Assert.assertTrue(containsOtp(codeInSentenceAfterNewline))
        Assert.assertFalse(containsOtp(codeTwoSentencesAfterContext))
        Assert.assertFalse(containsOtp(codeInSentenceBeforeContext))
        Assert.assertFalse(containsOtp(codeInNextSentenceTooFar))
        Assert.assertFalse(containsOtp(codeTooFarBeforeContext))
    }
}
