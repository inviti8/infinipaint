#pragma once

namespace GUIStuff { namespace ElementHelpers {

// FIXME: Currently, when two scroll areas in popups are created in the "same place" (different ids), only the first scroll area that's displayed will
// have scrolling capabilities, while the second scroll area wont get any scroll data (Clay_GetScrollContainerData gives 0s for content and container dimensions)
// We can workaround this by adding invisible padding to the second scroll area (which is what this function does), but ideally this should be fixed later
void SCROLL_AREA_BUG_WORKAROUND();

}}
