// SPDX-License-Identifier: MIT
#pragma once

#include "OpenGolfAccountStyle.h"
#include "Widgets/SNullWidget.h"

/** The title row of a menu page: a back icon button, the title, and an optional control on the right. */
namespace PageHeader
{
    /** Tag on the back control, so tests and tools can find it. */
    inline FName BackTag() { return TEXT("PageBack"); }

    TSharedRef<SWidget> Make(const FOpenGolfAccountStyle& Style, const TAttribute<FText>& Title, FSimpleDelegate OnBack,
        TSharedRef<SWidget> Right = SNullWidget::NullWidget);
} // namespace PageHeader
