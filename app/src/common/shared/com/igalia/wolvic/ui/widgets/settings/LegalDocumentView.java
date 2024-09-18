package com.igalia.wolvic.ui.widgets.settings;

import android.content.Context;
import android.graphics.Point;
import android.view.LayoutInflater;

import androidx.databinding.DataBindingUtil;

import com.igalia.wolvic.R;
import com.igalia.wolvic.databinding.LegalLicenseContentBinding;
import com.igalia.wolvic.databinding.OptionsLegalDocumentBinding;
import com.igalia.wolvic.ui.widgets.WidgetManagerDelegate;
import com.igalia.wolvic.ui.widgets.WidgetPlacement;

/**
 * A SettingsView that displays a legal document like the Terms of Service or the Privacy Policy,.
 * The content itself is shared with LegalDocumentDialogWidget.
 */
public class LegalDocumentView extends SettingsView {

    public enum LegalDocument {
        TERMS_OF_SERVICE,
        PRIVACY_POLICY,
        LEGAL_LICENSE
    }

    private LegalDocument mLegalDocument;
    private OptionsLegalDocumentBinding mBinding;

    private LegalLicenseContentBinding mLicenseBinding;

    public LegalDocumentView(Context aContext, WidgetManagerDelegate aWidgetManager, LegalDocument legalDocument) {
        super(aContext, aWidgetManager);
        this.mLegalDocument = legalDocument;
        initialize(aContext);
    }

    private void initialize(Context aContext) {
        updateUI();
    }

    @Override
    protected void updateUI() {
        super.updateUI();

        LayoutInflater inflater = LayoutInflater.from(getContext());

        // Inflate this data binding layout
        mBinding = DataBindingUtil.inflate(inflater, R.layout.options_legal_document, this, true);

        // Inflate the specific content
        switch (mLegalDocument){
            case TERMS_OF_SERVICE:
                inflater.inflate(R.layout.terms_service_content, mBinding.scrollbar, true);
                mBinding.headerLayout.setTitle(R.string.settings_terms_service);
                break;
            case PRIVACY_POLICY:
                inflater.inflate(R.layout.privacy_policy_content, mBinding.scrollbar, true);
                mBinding.headerLayout.setTitle(R.string.settings_privacy_policy);
                break;
            case LEGAL_LICENSE:
                mLicenseBinding = DataBindingUtil.inflate(inflater, R.layout.legal_license_content, mBinding.scrollbar, true);
                mBinding.headerLayout.setTitle(R.string.settings_legal_license);
                if(mLicenseBinding != null){
                    mLicenseBinding.legalLicenseSection2.setLinkClickListener((widget, url) -> {
                        mWidgetManager.openNewTabForeground(url);
                        exitWholeSettings();
                    });
                }
                break;
            default:
                break;
        }

        mScrollbar = mBinding.scrollbar;



        mBinding.headerLayout.setBackClickListener(view -> {
            if (mDelegate != null)
                mDelegate.showView(SettingViewType.PRIVACY);
        });
    }

    @Override
    public Point getDimensions() {
        return new Point(WidgetPlacement.dpDimension(getContext(), R.dimen.settings_dialog_width),
                WidgetPlacement.dpDimension(getContext(), R.dimen.privacy_options_height));
    }

    @Override
    protected SettingViewType getType() {
        switch (mLegalDocument) {
            case TERMS_OF_SERVICE:
                return SettingViewType.TERMS_OF_SERVICE;
            case PRIVACY_POLICY:
                return SettingViewType.PRIVACY_POLICY;
        }
        return SettingViewType.LEGAL_LICENSE;
    }
}
