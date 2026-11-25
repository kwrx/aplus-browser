/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2007 Andrea Anzani <andrea.anzani@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "EditorClientAplus.h"

#include <WebCore/Document.h>
#include <WebCore/DocumentPage.h>
#include <WebCore/Editor.h>
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameInlines.h>
#include <WebCore/FrameSelection.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/Settings.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/WindowsKeyboardCodes.h>

#include "AplusWebView.h"

namespace WebCore {

EditorClientAplus::EditorClientAplus(AplusWebView* view)
    : m_view(view)
    , m_isInRedo(false)
{
}

bool EditorClientAplus::shouldDeleteRange(const std::optional<SimpleRange>& range)
{
    return true;
}

bool EditorClientAplus::smartInsertDeleteEnabled()
{
    notImplemented();
    return false;
}

bool EditorClientAplus::isContinuousSpellCheckingEnabled()
{
    notImplemented();
    return false;
}

void EditorClientAplus::toggleContinuousSpellChecking()
{
    notImplemented();
}

bool EditorClientAplus::isGrammarCheckingEnabled()
{
    notImplemented();
    return false;
}

void EditorClientAplus::toggleGrammarChecking()
{
    notImplemented();
}

int EditorClientAplus::spellCheckerDocumentTag()
{
    notImplemented();
    return 0;
}

bool EditorClientAplus::shouldBeginEditing(const SimpleRange& range)
{
    return true;
}

bool EditorClientAplus::shouldEndEditing(const SimpleRange& range)
{
    return true;
}

bool EditorClientAplus::shouldInsertNode(Node& node, const std::optional<SimpleRange>& range, EditorInsertAction action)
{
    return true;
}

bool EditorClientAplus::shouldInsertText(const String& text, const std::optional<SimpleRange>& range, EditorInsertAction action)
{
    return true;
}

bool EditorClientAplus::shouldChangeSelectedRange(const std::optional<SimpleRange>& fromRange, const std::optional<SimpleRange>& toRange,
    Affinity affinity, bool stillSelecting)
{
    return true;
}

bool EditorClientAplus::shouldApplyStyle(const WebCore::StyleProperties& style,
                                      const std::optional<SimpleRange>& range)
{
    return true;
}

bool EditorClientAplus::shouldMoveRangeAfterDelete(const SimpleRange&, const SimpleRange&)
{
    notImplemented();
    return true;
}

void EditorClientAplus::didBeginEditing()
{
}

void EditorClientAplus::respondToChangedContents()
{
}

void EditorClientAplus::respondToChangedSelection(LocalFrame* frame)
{
    if (!frame)
        return;

    if (!frame->editor().ignoreSelectionChanges()) {
        auto& selection = frame->selection().selection();
        bool selectionIsPainted = selection.isRange() || (selection.isCaret() && selection.hasEditableStyle());

        if (m_lastSelectionWasPainted || selectionIsPainted) {
            if (auto* page = frame->page())
                page->scheduleRenderingUpdate({ RenderingUpdateStep::LayerFlush });
        }

        m_lastSelectionWasPainted = selectionIsPainted;
    }

}

void EditorClientAplus::didEndEditing()
{
}

void EditorClientAplus::didWriteSelectionToPasteboard()
{
    notImplemented();
}

bool EditorClientAplus::isSelectTrailingWhitespaceEnabled() const
{
    notImplemented();
	return false;
}

void EditorClientAplus::willWriteSelectionToPasteboard(const std::optional<SimpleRange>&)
{
}

void EditorClientAplus::discardedComposition(const Document&)
{
}

void EditorClientAplus::canceledComposition()
{
}

void EditorClientAplus::registerUndoStep(WebCore::UndoStep& step)
{
    if (!m_isInRedo)
        m_redoStack.clear();
    m_undoStack.append(&step);
}

void EditorClientAplus::registerRedoStep(WebCore::UndoStep& step)
{
    m_redoStack.append(&step);
}

void EditorClientAplus::clearUndoRedoOperations()
{
    m_undoStack.clear();
    m_redoStack.clear();
}

bool EditorClientAplus::canCopyCut(WebCore::LocalFrame*, bool defaultValue) const
{
    return defaultValue;
}

bool EditorClientAplus::canPaste(WebCore::LocalFrame*, bool defaultValue) const
{
    return defaultValue;
}

bool EditorClientAplus::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool EditorClientAplus::canRedo() const
{
    return !m_redoStack.isEmpty();
}

void EditorClientAplus::undo()
{
    if (canUndo()) {
        RefPtr<WebCore::UndoStep> step(m_undoStack.takeLast());
        // unapply will call us back to push this step onto the redo stack.
        step->unapply();
    }
}

void EditorClientAplus::redo()
{
    if (canRedo()) {
        RefPtr<WebCore::UndoStep> step(m_redoStack.takeLast());

        ASSERT(!m_isInRedo);
        m_isInRedo = true;
        // reapply will call us back to push this step onto the undo stack.
        step->reapply();
        m_isInRedo = false;
    }
}

void EditorClientAplus::handleKeyboardEvent(KeyboardEvent& event)
{
    const PlatformKeyboardEvent* platformEvent = event.underlyingPlatformEvent();
    if (!platformEvent || platformEvent->type() == PlatformEvent::Type::KeyUp)
        return;

    if (handleEditingKeyboardEvent(&event, platformEvent)) {
        event.setDefaultHandled();
    }
}

void EditorClientAplus::handleInputMethodKeydown(KeyboardEvent&)
{
    notImplemented();
}

void EditorClientAplus::textFieldDidBeginEditing(Element&)
{
}

void EditorClientAplus::textFieldDidEndEditing(Element&)
{
}

void EditorClientAplus::textDidChangeInTextField(Element&)
{
}

bool EditorClientAplus::doTextFieldCommandFromEvent(Element&, KeyboardEvent*)
{
    return false;
}

void EditorClientAplus::textWillBeDeletedInTextField(Element&)
{
    notImplemented();
}

void EditorClientAplus::textDidChangeInTextArea(Element&)
{
    notImplemented();
}

void EditorClientAplus::overflowScrollPositionChanged()
{
    notImplemented();
}

bool EditorClientAplus::shouldEraseMarkersAfterChangeSelection(TextCheckingType) const
{
    return true;
}

void EditorClientAplus::ignoreWordInSpellDocument(const String&)
{
    notImplemented();
}

void EditorClientAplus::learnWord(const String&)
{
    notImplemented();
}

void EditorClientAplus::checkSpellingOfString(StringView, int*, int*)
{
    notImplemented();
}

#if 0
String EditorClientAplus::getAutoCorrectSuggestionForMisspelledWord(const String& /*misspelledWord*/)
{
    notImplemented();
    return String();
}
#endif

void EditorClientAplus::checkGrammarOfString(StringView, Vector<GrammarDetail>&, int*, int*)
{
    notImplemented();
}

void EditorClientAplus::getGuessesForWord(const String&, const String&, const WebCore::VisibleSelection&, Vector<String>&)
{
    notImplemented();
}

void EditorClientAplus::requestCheckingOfString(TextCheckingRequest&, const WebCore::VisibleSelection&)
{
    notImplemented();
}

void EditorClientAplus::requestExtendedCheckingOfString(TextCheckingRequest&, const VisibleSelection& currentSelection)
{
    notImplemented();
}

void EditorClientAplus::updateSpellingUIWithGrammarString(const String&, const GrammarDetail&)
{
    notImplemented();
}

void EditorClientAplus::updateSpellingUIWithMisspelledWord(const String&)
{
    notImplemented();
}

void EditorClientAplus::showSpellingUI(bool)
{
    notImplemented();
}

bool EditorClientAplus::spellingUIIsShowing()
{
    notImplemented();
    return false;
}

void EditorClientAplus::setInputMethodState(Element*)
{
    notImplemented();
}

bool EditorClientAplus::performTwoStepDrop(DocumentFragment&, const SimpleRange& destination, bool isMove)
{
	notImplemented();
	return false;
}

// #pragma mark -

bool EditorClientAplus::handleEditingKeyboardEvent(KeyboardEvent* event,
    const PlatformKeyboardEvent* platformEvent)
{
    LocalFrame* frame = m_view->page()->focusController().focusedOrMainFrame();
    if (!frame || !frame->document())
        return false;

    // TODO be more specific when filtering events here. Some of the keys are
    // relevant only when there is a range-selection (shift+arrows to edit it),
    // but others are only relevant when there is an edition going on
    // (backspace, delete). We should check these different cases depending on
    // the key, and decide to swallow the event (return true) or not, in which
    // case the BWebFrame code can handle it for scrolling or other keyboard
    // shortcuts.
    if (!frame->selection().isRange() && !frame->editor().canEdit())
        return false;

    switch (platformEvent->windowsVirtualKeyCode()) {
    case VK_BACK:
        frame->editor().deleteWithDirection(SelectionDirection::Backward,
            platformEvent->controlKey() ? TextGranularity::WordGranularity
                : TextGranularity::CharacterGranularity,
            false, true);
        break;
    case VK_DELETE:
        frame->editor().deleteWithDirection(SelectionDirection::Forward,
            platformEvent->controlKey() ? TextGranularity::WordGranularity
                : TextGranularity::CharacterGranularity,
            false, true);
        break;
    case VK_LEFT:
        frame->selection().modify(platformEvent->shiftKey()
                ? FrameSelection::Alteration::Extend : FrameSelection::Alteration::Move,
            SelectionDirection::Left,
            platformEvent->controlKey() ? TextGranularity::WordGranularity
                : TextGranularity::CharacterGranularity,
            UserTriggered::Yes);
        break;
    case VK_RIGHT:
        frame->selection().modify(platformEvent->shiftKey() ? FrameSelection::Alteration::Extend
                : FrameSelection::Alteration::Move,
            SelectionDirection::Right,
            platformEvent->controlKey() ? TextGranularity::WordGranularity
                : TextGranularity::CharacterGranularity,
            UserTriggered::Yes);
        break;
    case VK_UP:
        frame->selection().modify(platformEvent->shiftKey() ? FrameSelection::Alteration::Extend
                : FrameSelection::Alteration::Move,
            SelectionDirection::Backward,
            platformEvent->controlKey() ? TextGranularity::ParagraphGranularity
                : TextGranularity::LineGranularity,
            UserTriggered::Yes);
        break;
    case VK_DOWN:
        frame->selection().modify(platformEvent->shiftKey() ? FrameSelection::Alteration::Extend
                : FrameSelection::Alteration::Move,
            SelectionDirection::Forward,
            platformEvent->controlKey() ? TextGranularity::ParagraphGranularity
                : TextGranularity::LineGranularity,
            UserTriggered::Yes);
        break;
    case VK_HOME:
        if (platformEvent->shiftKey() && platformEvent->controlKey())
            frame->editor().command(ASCIILiteral::fromLiteralUnsafe("MoveToBeginningOfDocumentAndModifySelection")).execute();
        else if (platformEvent->shiftKey())
            frame->editor().command(ASCIILiteral::fromLiteralUnsafe("MoveToBeginningOfLineAndModifySelection")).execute();
        else if (platformEvent->controlKey())
            frame->editor().command(ASCIILiteral::fromLiteralUnsafe("MoveToBeginningOfDocument")).execute();
        else
            frame->editor().command(ASCIILiteral::fromLiteralUnsafe("MoveToBeginningOfLine")).execute();
        break;
    case VK_END:
        if (platformEvent->shiftKey() && platformEvent->controlKey())
            frame->editor().command(ASCIILiteral::fromLiteralUnsafe("MoveToEndOfDocumentAndModifySelection")).execute();
        else if (platformEvent->shiftKey())
            frame->editor().command(ASCIILiteral::fromLiteralUnsafe("MoveToEndOfLineAndModifySelection")).execute();
        else if (platformEvent->controlKey())
            frame->editor().command(ASCIILiteral::fromLiteralUnsafe("MoveToEndOfDocument")).execute();
        else
            frame->editor().command(ASCIILiteral::fromLiteralUnsafe("MoveToEndOfLine")).execute();
        break;
    case VK_PRIOR:  // PageUp
        if (platformEvent->shiftKey())
            frame->editor().command(ASCIILiteral::fromLiteralUnsafe("MovePageUpAndModifySelection")).execute();
        else
            frame->editor().command(ASCIILiteral::fromLiteralUnsafe("MovePageUp")).execute();
        break;
    case VK_NEXT:  // PageDown
        if (platformEvent->shiftKey())
            frame->editor().command(ASCIILiteral::fromLiteralUnsafe("MovePageDownAndModifySelection")).execute();
        else
            frame->editor().command(ASCIILiteral::fromLiteralUnsafe("MovePageDown")).execute();
        break;
    case VK_RETURN:
        if (platformEvent->shiftKey())
            frame->editor().command(ASCIILiteral::fromLiteralUnsafe("InsertLineBreak")).execute();
        else
            frame->editor().command(ASCIILiteral::fromLiteralUnsafe("InsertNewline")).execute();
        break;
    case VK_TAB:
        return false;
    default:
        if (!platformEvent->controlKey() && !platformEvent->altKey() && !platformEvent->text().isEmpty()) {
            if (platformEvent->text().length() == 1) {
                UChar ch = platformEvent->text()[0];
                // Don't insert null or control characters as they can result in unexpected behaviour
                if (ch < ' ')
                    break;
            }
            frame->editor().insertText(platformEvent->text(), event);
        } else if (platformEvent->controlKey()) {
            switch (platformEvent->windowsVirtualKeyCode()) {
            case VK_B:
                frame->editor().command(ASCIILiteral::fromLiteralUnsafe("ToggleBold")).execute();
                break;
            case VK_I:
                frame->editor().command(ASCIILiteral::fromLiteralUnsafe("ToggleItalic")).execute();
                break;
            case VK_V:
                frame->editor().command(ASCIILiteral::fromLiteralUnsafe("Paste")).execute();
                break;
            case VK_X:
                frame->editor().command(ASCIILiteral::fromLiteralUnsafe("Cut")).execute();
                break;
            case VK_Y:
            case VK_Z:
                if (platformEvent->shiftKey())
                    frame->editor().command(ASCIILiteral::fromLiteralUnsafe("Redo")).execute();
                else
                    frame->editor().command(ASCIILiteral::fromLiteralUnsafe("Undo")).execute();
                break;
            case VK_A:
                frame->editor().command(ASCIILiteral::fromLiteralUnsafe("SelectAll")).execute();
                break;
            case VK_C:
                frame->editor().command(ASCIILiteral::fromLiteralUnsafe("Copy")).execute();
                break;
            default:
                return false;
            }
        } else
            return false;
    }

    return true;
}

void EditorClientAplus::setPendingComposition(const char* newComposition)
{
    m_pendingComposition = newComposition ? newComposition : "";
}

void EditorClientAplus::setPendingPreedit(const char* newPreedit)
{
    m_pendingPreedit = newPreedit ? newPreedit : "";
}

void EditorClientAplus::clearPendingIMData()
{
    setPendingComposition(nullptr);
    setPendingPreedit(nullptr);
}

void EditorClientAplus::imContextCommitted(const char* str, EditorClient* /*client*/)
{
    // This signal will fire during a keydown event. We want the contents of the
    // field to change right before the keyup event, so we wait until then to actually
    // commit this composition.
    setPendingComposition(str);
}

void EditorClientAplus::imContextPreeditChanged(EditorClient* /*client*/)
{
    const char* newPreedit = 0;
    // FIXME: get pre edit
    setPendingPreedit(newPreedit);
}


} // namespace WebCore

