#!/usr/bin/env python3
"""Gate the emitted Xtensa hot path, not the entire SDK's stack catalogue.

Usage: check_vertical_stack.py BUILD_DIRECTORY [--output RESULT.json]
Missing/ambiguous frames and unbounded dynamic stack fail closed. The nested
budget covers the reproduced A/B rendering path; it is not a whole-program
worst-case proof and does not replace device low-water measurements.
"""
import argparse
import json
from pathlib import Path
import re

RULES = {
    'paintVerticalView': (r':s3app::VerticalPaintResult s3app::paintVerticalView\(', 448),
    'draw': (r':void \{anonymous\}::draw\(\)$', 1280),
    'deviceLoop': (r':void deviceLoop\(\)$', 896),
    'verticalIme': (r':ime_device::VerticalImeFrame ime_device::verticalIme\(', 512),
    'layoutAb': (r':draftor::vertical::AbResult draftor::vertical::layoutAb\(', 160),
    'layoutAbImpl': (r':draftor::vertical::AbResult draftor::vertical::layoutAbImpl\(', 320),
    'paintAbRun': (r':draftor::vertical::PaintResult draftor::vertical::paintAbRun\(', 144),
    'paintOne': (r':draftor::vertical::paintAbRun.*::<lambda\(uint32_t, bool, bool, unsigned int\)>$', 160),
    'scalar': (r'vertical_ab\.cpp:\d+:\d+:constprop\(std::string_view, std::size_t, uint32_t&\)', 80),
    'presentContext': (r':void \{anonymous\}::_ZN12_GLOBAL__N_114presentContextEv\$part\$0\(\)$', 1024),
    'loop': (r':void loop\(\)$', 32),
    'compositionRuns': (r':draftor::vertical::AbStatus draftor::vertical::compositionRuns\(', 256),
    'paintHorizontalRaw': (r':void s3app::paintHorizontalRaw\(', 512),
    'compositionGlyph': (r':virtual s3host::GlyphView s3app::ImeCompositionFont::glyph\(', 96),
}

# Inline/COMDAT helpers can be emitted in several product consumers. Check
# every emitted instance and budget the maximum, rather than silently ignoring
# a large out-of-line copy or demanding a unique weak definition.
R3_RULES = {
 'prepareIme':(r'::prepareImePresentation\(',96),
 'controllerPrepare':(r'::PresentationController::prepare\(',256),
 'controllerSlots':(r'::PresentationController::slots\(',96),
 'verticalRegion':(r'::VerticalPresentation::setRegion\(',128),
 'verticalPlace':(r'::VerticalPresentation::placeCaretSlot\(',96),
 'verticalSettle':(r'::VerticalPresentation::settle\(',96),
 'verticalService':(r'::VerticalPresentation::service\(',192),
 'verticalRefresh':(r'::VerticalPresentation::refresh\(',256),
 'viewportStart':(r'::Viewport::start\(',256),
 'viewportService':(r'::Viewport::service\(',256),
 'viewportLocate':(r'::Viewport::locateUnchecked\(',240),
 'flowStep':(r'::Flow::step\(',192),
 'horizontalPlace':(r'::VisualCursorNavigator::placeCaretSlot\(',96),
 'horizontalSync':(r'::VisualCursorNavigator::sync\(',128),
 'horizontalAnchor':(r'::VisualCursorNavigator::anchorForLine\(',160),
 'horizontalCaret':(r'::VisualCursorNavigator::caretAtByte\(',128),
 'horizontalCurrent':(r'::VisualCursorNavigator::currentCaret\(',80),
 'horizontalEnsure':(r'::VisualCursorNavigator::ensureVisible\(',80),
 'layoutLine':(r'::LayoutEngine::layoutVisualLine\(',160),
 'layoutCaret':(r'::LayoutEngine::visualCaretAtFromAnchor\(',160),
 'navigationReady':(r'::DeviceIme::navigationReady\(',224),
 'compositionWidth':(r'::requiredCompositionWidth\(',64),
 'compositionRowWidth':(r'::compositionRowWidth\(',96),
 'finishScan':(r'::CandidateWindow::finishScan\(',80),
 'finishRow':(r'::CandidateWindow::finishRow\(',32),
}

# R3-R1 bounded presentation helpers. Existing frame/loop/nested limits above
# are unchanged. These are additional fail-closed obligations, not exclusions.
R1_RULES = {
 'observeComposition':(r'::DeviceIme::observeCompositionPresentation\(',224),
 'focusedCurrent':(r'::CandidateWindow::focusedCurrent\(',160),
 'candidateCurrent':(r'::CandidateWindow::current\(',128),
 'candidateProjection':(r'::verticalCandidateView\(',96),
 'compositionRow':(r':void draftor::vertical::compositionRow\(',256),
 'selectedRuns':(r':void ime_device::selectedRuns\(',224),
 'selectedViewport':(r'::SelectedViewport::update\(.*\)$',128),
 'setPacking':(r'::CandidateWindow::set(?:VerticalPacking|Packing)\(',80),
 'cachedItem':(r'::CandidateWindow::cachedItem\(',160),
 'beginText':(r'::CandidateWindow::beginText\(',640),
 'candidateRequest':(r'::CandidateWindow::request\(',96),
 'candidateContinue':(r'::CandidateWindow::continueScan\(',64),
 'candidateService':(r'::CandidateWindow::service\(',240),
 'appendText':(r'::CandidateWindow::appendText\(',96),
 'spatialNavigate':(r'::DeviceIme::spatialNavigate\(',192),
 'applySpatial':(r'::DeviceIme::applySpatial\(',64),
 'serviceArrows':(r'::DeviceIme::serviceArrows\(',112),
 'imeEvent':(r'::DeviceIme::event\(',128),
 'imeDispatch':(r'::DeviceIme::dispatch\(',144),
 'imeService':(r'::DeviceIme::service\(',576),
 'hudAppend':(r':void s3app::hudAppend\(',128),
 'verticalStatus':(r':const char\* ime_device::verticalStatus\(',96),
}

def inspect(root):
    rows=[]
    for path in sorted(root.rglob('*.su')):
        for line in path.read_text().splitlines():
            parts=line.split('\t')
            if len(parts)==3:
                rows.append(dict(symbol=parts[0],bytes=int(parts[1]),kind=parts[2],file=str(path)))
    failures=[];frames={}
    for name,(pattern,limit) in RULES.items():
        matches=[r for r in rows if re.search(pattern,r['symbol'])]
        if len(matches)!=1:
            failures.append(f'{name}: expected one emitted frame, found {len(matches)}');continue
        frames[name]=matches[0]
        if matches[0]['bytes']>limit or matches[0]['kind']!='static':
            failures.append(f'{name}: {matches[0]["bytes"]} B {matches[0]["kind"]}; limit {limit} B static')
    for name,(pattern,limit) in {**R3_RULES,**R1_RULES}.items():
        matches=[r for r in rows if re.search(pattern,r['symbol'])]
        if not matches:failures.append(name+': missing R3 helper');continue
        frames[name]=dict(max(matches,key=lambda r:r['bytes']),emitted_instances=len(matches))
        if any(r['kind']!='static' or r['bytes']>limit for r in matches):failures.append(name+': R3 helper exceeds budget')
    # The callback bridges are emitted functions and stay alive during painting.
    for name,pattern,limit in [
        ('ab_callback',r':static constexpr void s3app::paintVerticalView.*::<lambda\(void\*, const draftor::vertical::AbRun&\)>.*::_FUN',64),
        ('pixel_callback',r':static constexpr void s3app::paintVerticalView.*::<lambda\(void\*, uint16_t, uint16_t\)>.*::_FUN',48),
        ('device_pixel',r':static constexpr void \{anonymous\}::draw.*::<lambda\(void\*, uint16_t, uint16_t, s3app::VerticalColor\)>.*::_FUN',48),
    ]:
        matches=[r for r in rows if re.search(pattern,r['symbol'])]
        if not matches:failures.append(name+': missing callback');continue
        frames[name]=max(matches,key=lambda r:r['bytes'])
        if any(r['kind']!='static' or r['bytes']>limit for r in matches):failures.append(name+': callback exceeds budget')
    nested={}
    if all(k in frames for k in RULES) and all(k in frames for k in ('ab_callback','pixel_callback','device_pixel')):
        # SDK wrapper + Arduino loopTask: 32 each, verified from linked ELF.
        prefix=64+sum(frames[k]['bytes'] for k in ('loop','deviceLoop','draw','paintVerticalView','verticalIme','layoutAb','layoutAbImpl','ab_callback','paintAbRun'))
        nested['reproduced_scalar_path']=prefix+frames['scalar']['bytes']
        # Same caller prefix; scalar and paintOne are sequential, not nested.
        # Reserve 256 B for the driver pixel/font leaf branch (linked review).
        nested['ab_raster_branch_budget']=prefix+frames['paintOne']['bytes']+frames['pixel_callback']['bytes']+frames['device_pixel']['bytes']+256
        nested['composition_raster_budget']=prefix+frames['compositionRuns']['bytes']+frames.get('compositionRow',{}).get('bytes',0)+frames['pixel_callback']['bytes']+frames['device_pixel']['bytes']+256
        nested['context_rebind_ab_budget']=max(nested['ab_raster_branch_budget'],nested['composition_raster_budget'])+frames['presentContext']['bytes']
        nested['horizontal_raw_budget']=64+sum(frames[k]['bytes'] for k in ('loop','deviceLoop','draw','paintHorizontalRaw'))+768
        nested['context_rebind_horizontal_budget']=nested['horizontal_raw_budget']+frames['presentContext']['bytes']
        if all(k in frames for k in R3_RULES):
            size=lambda *names:sum(frames[k]['bytes'] for k in names)
            prepare=64+size('loop','deviceLoop','draw','prepareIme','controllerPrepare')
            locate=max(size('verticalRefresh','viewportStart','viewportLocate'),
                       size('viewportService','viewportLocate'),size('viewportService','flowStep')+256)
            v=prepare+max(size('verticalRegion'),size('controllerSlots'),size('verticalPlace'))+size('verticalSettle','verticalService')+locate
            h=prepare+max(size('controllerSlots','horizontalSync','horizontalCurrent','horizontalCaret','layoutCaret'),
                          size('horizontalPlace','horizontalAnchor','layoutLine'),
                          size('horizontalPlace','horizontalSync','horizontalEnsure','horizontalAnchor','layoutLine'))+768
            nested['R3_vertical_prepare_with_context']=v+size('presentContext')
            nested['R3_horizontal_anchor_with_context']=h+size('presentContext')
            # navigationReady adds only this bounded read after existing analysis;
            # it neither nests another analysis nor calls the presentation painter.
            nested['R3_peak_read_increment']=size('navigationReady','observeComposition','compositionWidth','compositionRowWidth','compositionGlyph')
        if all(k in frames for k in R1_RULES) and all(k in frames for k in R3_RULES):
            size=lambda *names:sum(frames[k]['bytes'] for k in names)
            root=64+size('loop','deviceLoop')
            paint=root+size('draw','paintVerticalView','verticalIme','selectedRuns')
            nested['R1_selected_viewport_raster_with_context']=paint+max(size('selectedViewport')+128,size('ab_callback','paintAbRun','paintOne','pixel_callback','device_pixel')+256)+size('presentContext')
            nested['R1_prepare_repacking_with_context']=root+size('draw','prepareIme','setPacking','beginText','cachedItem','presentContext')+768
            nested['R1_spatial_navigation']=root+size('imeEvent','imeDispatch','spatialNavigate','serviceArrows','applySpatial','candidateRequest','candidateContinue','beginText','cachedItem')+1024
            nested['R1_cached_materialization']=root+size('imeService','candidateService','appendText','candidateContinue','beginText','cachedItem')+1024
            nested['R2_projection_with_context']=root+size('draw','paintVerticalView','verticalIme')+max(size('focusedCurrent','candidateCurrent'),size('candidateProjection'))+size('presentContext')+256
            nested['SPACE02_complete_window']=root+size('imeService','candidateService','appendText','finishRow','finishScan','focusedCurrent','candidateCurrent')+256
            nested['R2_observation']=root+size('imeService','navigationReady','observeComposition','compositionWidth','compositionRowWidth','compositionGlyph')+256
            nested['R1_hud_with_context']=root+size('draw','hudAppend','verticalStatus','presentContext')+256
        if max(nested.values())>5632:failures.append('active-IME nested path exceeds 5632 B budget')
    return dict(passed=not failures,failures=failures,frames=frames,nested=nested,
                scope='Checked S/A/B raster, R3 preparation/anchor, R1 repacking/semantic keys/HUD and peak read paths; 64 B linked SDK wrappers; V leaf reserve 256 B, H getter/raster reserve 768 B; bounded critical-path budget, not whole-program peak; runtime headroom still required')

def main():
    parser=argparse.ArgumentParser();parser.add_argument('build',type=Path);parser.add_argument('--output',type=Path);args=parser.parse_args()
    result=inspect(args.build)
    if args.output:args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2));return 0 if result['passed'] else 1

if __name__=='__main__':raise SystemExit(main())
